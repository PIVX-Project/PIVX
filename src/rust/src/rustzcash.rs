// Catch documentation errors caused by code changes.
#![deny(broken_intra_doc_links)]

use bellman::{
    gadgets::multipack,
    groth16::{create_random_proof, verify_proof, Parameters, PreparedVerifyingKey, Proof},
};
use blake2s_simd::Params as Blake2sParams;
use bls12_381::{Bls12, Scalar};
use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};
use ff::{Field, PrimeField};
use group::GroupEncoding;
use jubjub::Fr;
use libc::{c_char, c_uchar, size_t};
use rand_core::OsRng;
use std::{
    ffi::CStr,
    fs::File,
    io::BufReader,
    ops::Mul,
    path::{Path, PathBuf},
    slice,
};
use zcash_note_encryption::Domain;
use zcash_primitives::{
    block::equihash,
    consensus::TestNetwork,
    constants::{CRH_IVK_PERSONALIZATION, PROOF_GENERATION_KEY_GENERATOR, SPENDING_KEY_GENERATOR},
    merkle_tree::MerklePath,
    sapling::{
        keys::{DiversifiedTransmissionKey, EphemeralSecretKey, NullifierDerivingKey},
        merkle_hash,
        note::ExtractedNoteCommitment,
        note_encryption::SaplingDomain,
        redjubjub::{
            Signature, {self},
        },
        spend_sig,
        value::{NoteValue, ValueCommitment},
        Diversifier, Note, PaymentAddress, ProofGenerationKey, Rseed,
    },
    transaction::components::Amount,
    zip32,
};
use zcash_proofs::{
    circuit::sprout,
    load_parameters,
    sapling::{SaplingProvingContext, SaplingVerificationContext},
    ZcashParameters,
};

#[cfg(not(target_os = "windows"))]
use std::ffi::OsStr;
#[cfg(not(target_os = "windows"))]
use std::os::unix::ffi::OsStrExt;

#[cfg(target_os = "windows")]
use std::ffi::OsString;
#[cfg(target_os = "windows")]
use std::os::windows::ffi::OsStringExt;

#[cfg(test)]
mod tests;

pub const SAPLING_TREE_DEPTH: usize = 32;
pub const SPROUT_TREE_DEPTH: usize = 29;

static mut SAPLING_SPEND_VK: Option<PreparedVerifyingKey<Bls12>> = None;
static mut SAPLING_OUTPUT_VK: Option<PreparedVerifyingKey<Bls12>> = None;
static mut SPROUT_GROTH16_VK: Option<PreparedVerifyingKey<Bls12>> = None;

static mut SAPLING_SPEND_PARAMS: Option<Parameters<Bls12>> = None;
static mut SAPLING_OUTPUT_PARAMS: Option<Parameters<Bls12>> = None;
static mut SPROUT_GROTH16_PARAMS_PATH: Option<PathBuf> = None;

/// Reads an FrRepr from [u8] of length 32
/// This will panic (abort) if length provided is
/// not correct
fn read_fs(from: &[u8]) -> Option<Fr> {
    assert_eq!(from.len(), 32);

    let f = Fr::from_bytes(from.try_into().ok()?);

    if f.is_some().into() {
        Some(f.unwrap())
    } else {
        None
    }
}

#[cfg(not(target_os = "windows"))]
#[no_mangle]
pub extern "system" fn librustzcash_init_zksnark_params(
    spend_path: *const u8,
    spend_path_len: usize,
    spend_hash: *const c_char,
    output_path: *const u8,
    output_path_len: usize,
    output_hash: *const c_char,
    sprout_path: *const u8,
    sprout_path_len: usize,
    sprout_hash: *const c_char,
) {
    let spend_path = Path::new(OsStr::from_bytes(unsafe {
        slice::from_raw_parts(spend_path, spend_path_len)
    }));
    let output_path = Path::new(OsStr::from_bytes(unsafe {
        slice::from_raw_parts(output_path, output_path_len)
    }));
    let sprout_path = if sprout_path.is_null() {
        None
    } else {
        Some(Path::new(OsStr::from_bytes(unsafe {
            slice::from_raw_parts(sprout_path, sprout_path_len)
        })))
    };

    init_zksnark_params(
        spend_path,
        spend_hash,
        output_path,
        output_hash,
        sprout_path,
        sprout_hash,
    )
}

#[cfg(target_os = "windows")]
#[no_mangle]
pub extern "system" fn librustzcash_init_zksnark_params(
    spend_path: *const u16,
    spend_path_len: usize,
    spend_hash: *const c_char,
    output_path: *const u16,
    output_path_len: usize,
    output_hash: *const c_char,
    sprout_path: *const u16,
    sprout_path_len: usize,
    sprout_hash: *const c_char,
) {
    let spend_path =
        OsString::from_wide(unsafe { slice::from_raw_parts(spend_path, spend_path_len) });
    let output_path =
        OsString::from_wide(unsafe { slice::from_raw_parts(output_path, output_path_len) });
    let sprout_path = if sprout_path.is_null() {
        None
    } else {
        Some(OsString::from_wide(unsafe {
            slice::from_raw_parts(sprout_path, sprout_path_len)
        }))
    };

    init_zksnark_params(
        Path::new(&spend_path),
        spend_hash,
        Path::new(&output_path),
        output_hash,
        sprout_path.as_ref().map(|p| Path::new(p)),
        sprout_hash,
    )
}

fn init_zksnark_params(
    spend_path: &Path,
    spend_hash: *const c_char,
    output_path: &Path,
    output_hash: *const c_char,
    sprout_path: Option<&Path>,
    sprout_hash: *const c_char,
) {
    let _spend_hash = unsafe { CStr::from_ptr(spend_hash) }
        .to_str()
        .expect("hash should be a valid string");

    let _output_hash = unsafe { CStr::from_ptr(output_hash) }
        .to_str()
        .expect("hash should be a valid string");

    let _sprout_hash = if sprout_path.is_none() {
        None
    } else {
        Some(
            unsafe { CStr::from_ptr(sprout_hash) }
                .to_str()
                .expect("hash should be a valid string"),
        )
    };

    // Load params
    let ZcashParameters {
        spend_params,
        spend_vk,
        output_params,
        output_vk,
        sprout_vk,
    } = load_parameters(spend_path, output_path, sprout_path);

    // Caller is responsible for calling this function once, so
    // these global mutations are safe.
    unsafe {
        SAPLING_SPEND_PARAMS = Some(spend_params);
        SAPLING_OUTPUT_PARAMS = Some(output_params);
        SPROUT_GROTH16_PARAMS_PATH = sprout_path.map(|p| p.to_owned());

        SAPLING_SPEND_VK = Some(spend_vk);
        SAPLING_OUTPUT_VK = Some(output_vk);
        SPROUT_GROTH16_VK = sprout_vk;
    }
}

#[no_mangle]
pub extern "system" fn librustzcash_tree_uncommitted(result: *mut [c_uchar; 32]) {
    let tmp = Fr::one();

    // Should be okay, caller is responsible for ensuring the pointer
    // is a valid pointer to 32 bytes that can be mutated.
    let result = unsafe { &mut *result };

    *result = tmp.to_bytes();
}

#[no_mangle]
pub extern "system" fn librustzcash_merkle_hash(
    depth: size_t,
    a: *const [c_uchar; 32],
    b: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) {
    let tmp = unsafe { merkle_hash(depth, &*a, &*b) };

    // Should be okay, caller is responsible for ensuring the pointer
    // is a valid pointer to 32 bytes that can be mutated.
    let result = unsafe { &mut *result };

    *result = tmp;
}

#[no_mangle] // ToScalar
pub extern "system" fn librustzcash_to_scalar(
    input: *const [c_uchar; 64],
    result: *mut [c_uchar; 32],
) {
    // Should be okay, because caller is responsible for ensuring
    // the pointer is a valid pointer to 32 bytes, and that is the
    // size of the representation
    let scalar = Fr::from_bytes_wide(&mut unsafe { *input });

    unsafe { *result = scalar.to_bytes() };
}

#[no_mangle]
pub extern "system" fn librustzcash_ask_to_ak(
    ask: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) {
    let ask = read_fs(unsafe { &*ask }).expect("ask should be a valid Fr");
    let ak = SPENDING_KEY_GENERATOR * ask;

    let result = unsafe { &mut *result };

    *result = ak.to_bytes();
}

#[no_mangle]
pub extern "system" fn librustzcash_nsk_to_nk(
    nsk: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) {
    let nsk = read_fs(unsafe { &*nsk }).expect("nsk should be a valid Fr");
    let nk = PROOF_GENERATION_KEY_GENERATOR * nsk;

    let result = unsafe { &mut *result };

    *result = nk.to_bytes();
}

#[no_mangle]
pub extern "system" fn librustzcash_crh_ivk(
    ak: *const [c_uchar; 32],
    nk: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) {
    let ak = unsafe { &*ak };
    let nk = unsafe { &*nk };

    let mut h = Blake2sParams::new()
        .hash_length(32)
        .personal(CRH_IVK_PERSONALIZATION)
        .to_state();
    h.update(ak);
    h.update(nk);
    let mut h = h.finalize().as_ref().to_vec();

    // Drop the last five bits, so it can be interpreted as a scalar.
    h[31] &= 0b0000_0111;

    let result = unsafe { &mut *result };

    result.copy_from_slice(&h);
}

#[no_mangle]
pub extern "system" fn librustzcash_check_diversifier(diversifier: *const [c_uchar; 11]) -> bool {
    let diversifier = Diversifier(unsafe { *diversifier });
    diversifier.g_d().is_some()
}

#[no_mangle]
pub extern "system" fn librustzcash_ivk_to_pkd(
    ivk: *const [c_uchar; 32],
    diversifier: *const [c_uchar; 11],
    result: *mut [c_uchar; 32],
) -> bool {
    let Some(ivk) = read_fs(unsafe { &*ivk }) else { return false };
    let diversifier = Diversifier(unsafe { *diversifier });
    if let Some(g_d) = diversifier.g_d() {
        let pk_d = g_d.mul(ivk);

        let result = unsafe { &mut *result };
        *result = pk_d.to_bytes();

        true
    } else {
        false
    }
}

/// Test generation of commitment randomness
#[test]
fn test_gen_r() {
    let mut r1 = [0u8; 32];
    let mut r2 = [0u8; 32];

    // Verify different r values are generated
    librustzcash_sapling_generate_r(&mut r1);
    librustzcash_sapling_generate_r(&mut r2);
    assert_ne!(r1, r2);

    // Verify r values are valid in the field
    let _ = jubjub::Scalar::from_bytes(&r1).unwrap();
    let _ = jubjub::Scalar::from_bytes(&r2).unwrap();
}

/// Return 32 byte random scalar, uniformly.
#[no_mangle]
pub extern "system" fn librustzcash_sapling_generate_r(result: *mut [c_uchar; 32]) {
    // create random 64 byte buffer
    let rng = OsRng;

    // reduce to uniform value
    let r = Fr::random(rng);
    let result = unsafe { &mut *result };
    *result = r.to_bytes();
}

// Private utility function to get Note from C parameters
fn priv_get_note(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    r: *const [c_uchar; 32],
) -> Result<Note, ()> {
    let mut bytes = [0; 43];
    bytes[0..11].copy_from_slice(&unsafe { *diversifier });
    bytes[11..].copy_from_slice(&unsafe { *pk_d });
    let Some(payment_addr) = PaymentAddress::from_bytes(&bytes) else { return Err(()) };

    // Deserialize randomness
    let r = {
        let r = Fr::from_repr(unsafe { *r });
        if r.is_some().into() {
            r.unwrap()
        } else {
            return Err(());
        }
    };

    let note = Note::from_parts(
        payment_addr,
        NoteValue::from_raw(value),
        Rseed::BeforeZip212(r),
    );

    Ok(note)
}

/// Compute Sapling note nullifier.
#[no_mangle]
pub extern "system" fn librustzcash_sapling_compute_nf(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    r: *const [c_uchar; 32],
    _ak: *const [c_uchar; 32],
    nk: *const [c_uchar; 32],
    position: u64,
    result: *mut [c_uchar; 32],
) -> bool {
    let note = match priv_get_note(diversifier, pk_d, value, r) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // The ak is unused to generate a nullifer.
    /*
    let ak = {
        let ak = jubjub::SubgroupPoint::from_bytes(&(unsafe { *ak }));
        if ak.is_some().into() {
            ak.unwrap()
        } else {
            return false;
        }
    };
     */
    let nk = {
        let nk = jubjub::SubgroupPoint::from_bytes(&(unsafe { *nk }));
        if nk.is_some().into() {
            NullifierDerivingKey(nk.unwrap())
        } else {
            return false;
        }
    };

    let nf = note.nf(&nk, position);
    let result = unsafe { &mut *result };
    result.copy_from_slice(&nf.0);

    true
}

/// Compute Sapling note commitment.
#[no_mangle]
pub extern "system" fn librustzcash_sapling_compute_cm(
    diversifier: *const [c_uchar; 11],
    pk_d: *const [c_uchar; 32],
    value: u64,
    r: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    let note = match priv_get_note(diversifier, pk_d, value, r) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let result = unsafe { &mut *result };
    *result = note.cmu().to_bytes();

    true
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_ka_agree(
    p: *const [c_uchar; 32],
    sk: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    let esk = {
        let esk = EphemeralSecretKey::from_bytes(&unsafe { *sk });
        if esk.is_some().into() {
            esk.unwrap()
        } else {
            return false;
        }
    };
    let p = {
        let p = DiversifiedTransmissionKey::from_bytes(&unsafe { *p });
        if p.is_some().into() {
            p.unwrap()
        } else {
            return false;
        }
    };

    let shared_secret = SaplingDomain::<TestNetwork>::ka_agree_enc(&esk, &p);
    unsafe { *result = shared_secret.0.to_bytes() };

    true
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_ka_derivepublic(
    diversifier: *const [c_uchar; 11],
    esk: *const [c_uchar; 32],
    result: *mut [c_uchar; 32],
) -> bool {
    let diversifier = Diversifier(unsafe { *diversifier });

    // Compute g_d from the diversifier
    let g_d = match diversifier.g_d() {
        Some(g) => g,
        None => return false,
    };

    // Deserialize esk
    let esk = {
        let esk = Fr::from_repr(unsafe { *esk });
        if esk.is_some().into() {
            esk.unwrap()
        } else {
            return false;
        }
    };

    let p = g_d.mul(esk);

    let result = unsafe { &mut *result };
    *result = p.to_bytes();

    true
}

#[no_mangle]
pub extern "system" fn librustzcash_eh_isvalid(
    n: u32,
    k: u32,
    input: *const c_uchar,
    input_len: size_t,
    nonce: *const c_uchar,
    nonce_len: size_t,
    soln: *const c_uchar,
    soln_len: size_t,
) -> bool {
    if (k >= n) || (n % 8 != 0) || (soln_len != (1 << k) * ((n / (k + 1)) as usize + 1) / 8) {
        return false;
    }
    let rs_input = unsafe { slice::from_raw_parts(input, input_len) };
    let rs_nonce = unsafe { slice::from_raw_parts(nonce, nonce_len) };
    let rs_soln = unsafe { slice::from_raw_parts(soln, soln_len) };
    equihash::is_valid_solution(n, k, rs_input, rs_nonce, rs_soln).is_ok()
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_verification_ctx_init(
) -> *mut SaplingVerificationContext {
    let ctx = Box::new(SaplingVerificationContext::new(true));

    Box::into_raw(ctx)
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_verification_ctx_free(
    ctx: *mut SaplingVerificationContext,
) {
    drop(unsafe { Box::from_raw(ctx) });
}

const GROTH_PROOF_SIZE: usize = 48 // π_A
    + 96 // π_B
    + 48; // π_C

#[no_mangle]
pub extern "system" fn librustzcash_sapling_check_spend(
    ctx: *mut SaplingVerificationContext,
    cv: *const [c_uchar; 32],
    anchor: *const [c_uchar; 32],
    nullifier: *const [c_uchar; 32],
    rk: *const [c_uchar; 32],
    zkproof: *const [c_uchar; GROTH_PROOF_SIZE],
    spend_auth_sig: *const [c_uchar; 64],
    sighash_value: *const [c_uchar; 32],
) -> bool {
    let _pcv = cv;
    // Deserialize the value commitment
    let cv = {
        let cv = ValueCommitment::from_bytes_not_small_order(&(unsafe { *cv }));
        if cv.is_some().into() {
            cv.unwrap()
        } else {
            return false;
        }
    };

    // Deserialize the anchor, which should be an element
    // of Fr.
    let anchor = {
        let anchor = Scalar::from_repr(unsafe { *anchor });
        if anchor.is_some().into() {
            anchor.unwrap()
        } else {
            return false;
        }
    };

    // Deserialize rk
    let rk = match redjubjub::PublicKey::read(&(unsafe { &*rk })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Deserialize the signature
    // Spend auth sig is not needed in shield stake proofs.
    // See #2836 for details
    let spend_auth_sig = if spend_auth_sig.is_null() {
        None
    } else {
        Signature::read(&(unsafe { &*spend_auth_sig })[..]).ok()
    };

    // Deserialize the proof
    let zkproof = match Proof::<Bls12>::read(&(unsafe { &*zkproof })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    let sighash_value = if sighash_value.is_null() {
        [0u8; 32]
    } else {
        unsafe { *sighash_value }
    };

    unsafe { &mut *ctx }.check_spend(
        &cv,
        anchor,
        unsafe { &*nullifier },
        rk.clone(),
        &sighash_value,
        spend_auth_sig,
        zkproof.clone(),
        unsafe { SAPLING_SPEND_VK.as_ref() }.unwrap(),
    )
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_check_output(
    ctx: *mut SaplingVerificationContext,
    cv: *const [c_uchar; 32],
    cm: *const [c_uchar; 32],
    epk: *const [c_uchar; 32],
    zkproof: *const [c_uchar; GROTH_PROOF_SIZE],
) -> bool {
    let _pcv = cv;
    // Deserialize the value commitment
    let cv = {
        let cv = ValueCommitment::from_bytes_not_small_order(&(unsafe { *cv }));
        if cv.is_some().into() {
            cv.unwrap()
        } else {
            return false;
        }
    };

    // Deserialize the commitment, which should be an element
    // of Fr.
    let cm = {
        let cm = ExtractedNoteCommitment::from_bytes(&(unsafe { *cm }));
        if cm.is_some().into() {
            cm.unwrap()
        } else {
            return false;
        }
    };

    // Deserialize the ephemeral key
    let epk = {
        let epk = jubjub::SubgroupPoint::from_bytes(&(unsafe { *epk }));
        if epk.is_some().into() {
            epk.unwrap()
        } else {
            return false;
        }
    };

    // Deserialize the proof
    let zkproof = match Proof::<Bls12>::read(&(unsafe { &*zkproof })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    unsafe { &mut *ctx }.check_output(
        &cv,
        cm,
        epk.into(),
        zkproof,
        unsafe { SAPLING_OUTPUT_VK.as_ref() }.unwrap(),
    )
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_final_check(
    ctx: *mut SaplingVerificationContext,
    value_balance: i64,
    binding_sig: *const [c_uchar; 64],
    sighash_value: *const [c_uchar; 32],
) -> bool {
    let value_balance = match Amount::from_i64(value_balance) {
        Ok(vb) => vb,
        Err(()) => return false,
    };

    // Deserialize the signature
    let binding_sig = match Signature::read(&(unsafe { &*binding_sig })[..]) {
        Ok(sig) => sig,
        Err(_) => return false,
    };

    // Sighash is not needed in Shield stake proof.
    // See #2836 for details.
    let sighash_value = if sighash_value.is_null() {
        [0u8; 32]
    } else {
        unsafe { *sighash_value }
    };

    unsafe { &*ctx }.final_check(value_balance, &sighash_value, binding_sig)
}

#[no_mangle]
pub extern "system" fn librustzcash_sprout_prove(
    proof_out: *mut [c_uchar; GROTH_PROOF_SIZE],

    phi: *const [c_uchar; 32],
    rt: *const [c_uchar; 32],
    h_sig: *const [c_uchar; 32],

    // First input
    in_sk1: *const [c_uchar; 32],
    in_value1: u64,
    in_rho1: *const [c_uchar; 32],
    in_r1: *const [c_uchar; 32],
    in_auth1: *const [c_uchar; 1 + 33 * SPROUT_TREE_DEPTH + 8],

    // Second input
    in_sk2: *const [c_uchar; 32],
    in_value2: u64,
    in_rho2: *const [c_uchar; 32],
    in_r2: *const [c_uchar; 32],
    in_auth2: *const [c_uchar; 1 + 33 * SPROUT_TREE_DEPTH + 8],

    // First output
    out_pk1: *const [c_uchar; 32],
    out_value1: u64,
    out_r1: *const [c_uchar; 32],

    // Second output
    out_pk2: *const [c_uchar; 32],
    out_value2: u64,
    out_r2: *const [c_uchar; 32],

    // Public value
    vpub_old: u64,
    vpub_new: u64,
) {
    let phi = unsafe { *phi };
    let rt = unsafe { *rt };
    let h_sig = unsafe { *h_sig };
    let in_sk1 = unsafe { *in_sk1 };
    let in_rho1 = unsafe { *in_rho1 };
    let in_r1 = unsafe { *in_r1 };
    let in_auth1 = unsafe { *in_auth1 };
    let in_sk2 = unsafe { *in_sk2 };
    let in_rho2 = unsafe { *in_rho2 };
    let in_r2 = unsafe { *in_r2 };
    let in_auth2 = unsafe { *in_auth2 };
    let out_pk1 = unsafe { *out_pk1 };
    let out_r1 = unsafe { *out_r1 };
    let out_pk2 = unsafe { *out_pk2 };
    let out_r2 = unsafe { *out_r2 };

    let mut inputs = Vec::with_capacity(2);
    {
        let mut handle_input = |sk, value, rho, r, mut auth: &[u8]| {
            let value = Some(value);
            let rho = Some(sprout::UniqueRandomness(rho));
            let r = Some(sprout::CommitmentRandomness(r));
            let a_sk = Some(sprout::SpendingKey(sk));

            // skip the first byte
            assert_eq!(auth[0], SPROUT_TREE_DEPTH as u8);
            auth = &auth[1..];

            let mut auth_path = [None; SPROUT_TREE_DEPTH];
            for i in (0..SPROUT_TREE_DEPTH).rev() {
                // skip length of inner vector
                assert_eq!(auth[0], 32);
                auth = &auth[1..];

                let mut sibling = [0u8; 32];
                sibling.copy_from_slice(&auth[0..32]);
                auth = &auth[32..];

                auth_path[i] = Some((sibling, false));
            }

            let mut position = auth
                .read_u64::<LittleEndian>()
                .expect("should have had index at the end");

            for i in 0..SPROUT_TREE_DEPTH {
                auth_path[i].as_mut().map(|p| p.1 = (position & 1) == 1);

                position >>= 1;
            }

            inputs.push(sprout::JsInput {
                value: value,
                a_sk: a_sk,
                rho: rho,
                r: r,
                auth_path: auth_path,
            });
        };

        handle_input(in_sk1, in_value1, in_rho1, in_r1, &in_auth1[..]);
        handle_input(in_sk2, in_value2, in_rho2, in_r2, &in_auth2[..]);
    }

    let mut outputs = Vec::with_capacity(2);
    {
        let mut handle_output = |a_pk, value, r| {
            outputs.push(sprout::JsOutput {
                value: Some(value),
                a_pk: Some(sprout::PayingKey(a_pk)),
                r: Some(sprout::CommitmentRandomness(r)),
            });
        };

        handle_output(out_pk1, out_value1, out_r1);
        handle_output(out_pk2, out_value2, out_r2);
    }

    let js = sprout::JoinSplit {
        vpub_old: Some(vpub_old),
        vpub_new: Some(vpub_new),
        h_sig: Some(h_sig),
        phi: Some(phi),
        inputs: inputs,
        outputs: outputs,
        rt: Some(rt),
    };

    // Load parameters from disk
    let sprout_fs = File::open(
        unsafe { &SPROUT_GROTH16_PARAMS_PATH }
            .as_ref()
            .expect("parameters should have been initialized"),
    )
    .expect("couldn't load Sprout groth16 parameters file");

    let mut sprout_fs = BufReader::with_capacity(1024 * 1024, sprout_fs);

    let params = Parameters::<Bls12>::read(&mut sprout_fs, false)
        .expect("couldn't deserialize Sprout JoinSplit parameters file");

    drop(sprout_fs);

    // Initialize secure RNG
    let mut rng = OsRng;

    let proof = create_random_proof(js, &params, &mut rng).expect("proving should not fail");

    proof
        .write(&mut (unsafe { &mut *proof_out })[..])
        .expect("should be able to serialize a proof");
}

#[no_mangle]
pub extern "system" fn librustzcash_sprout_verify(
    proof: *const [c_uchar; GROTH_PROOF_SIZE],
    rt: *const [c_uchar; 32],
    h_sig: *const [c_uchar; 32],
    mac1: *const [c_uchar; 32],
    mac2: *const [c_uchar; 32],
    nf1: *const [c_uchar; 32],
    nf2: *const [c_uchar; 32],
    cm1: *const [c_uchar; 32],
    cm2: *const [c_uchar; 32],
    vpub_old: u64,
    vpub_new: u64,
) -> bool {
    // Prepare the public input for the verifier
    let mut public_input = Vec::with_capacity((32 * 8) + (8 * 2));
    public_input.extend(unsafe { &(&*rt)[..] });
    public_input.extend(unsafe { &(&*h_sig)[..] });
    public_input.extend(unsafe { &(&*nf1)[..] });
    public_input.extend(unsafe { &(&*mac1)[..] });
    public_input.extend(unsafe { &(&*nf2)[..] });
    public_input.extend(unsafe { &(&*mac2)[..] });
    public_input.extend(unsafe { &(&*cm1)[..] });
    public_input.extend(unsafe { &(&*cm2)[..] });
    public_input.write_u64::<LittleEndian>(vpub_old).unwrap();
    public_input.write_u64::<LittleEndian>(vpub_new).unwrap();

    let public_input = multipack::bytes_to_bits(&public_input);
    let public_input = multipack::compute_multipacking::<Scalar>(&public_input);

    let proof = match Proof::read(unsafe { &(&*proof)[..] }) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Verify the proof
    match verify_proof(
        unsafe { SPROUT_GROTH16_VK.as_ref() }.expect("parameters should have been initialized"),
        &proof,
        &public_input[..],
    ) {
        // No error, and proof verification successful
        Ok(()) => true,

        // Any other case
        _ => false,
    }
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_output_proof(
    ctx: *mut SaplingProvingContext,
    esk: *const [c_uchar; 32],
    payment_address: *const [c_uchar; 43],
    rcm: *const [c_uchar; 32],
    value: u64,
    cv: *mut [c_uchar; 32],
    zkproof: *mut [c_uchar; GROTH_PROOF_SIZE],
) -> bool {
    // Grab `esk`, which the caller should have constructed for the DH key exchange.
    let esk = {
        let esk = Fr::from_repr(unsafe { *esk });
        if esk.is_some().into() {
            esk.unwrap()
        } else {
            return false;
        }
    };
    // Grab the payment address from the caller
    let payment_address = match PaymentAddress::from_bytes(unsafe { &*payment_address }) {
        Some(pa) => pa,
        None => return false,
    };

    // The caller provides the commitment randomness for the output note
    let rcm = {
        let rcm = Fr::from_repr(unsafe { *rcm });
        if rcm.is_some().into() {
            rcm.unwrap()
        } else {
            return false;
        }
    };

    // Create proof
    let (proof, value_commitment) = unsafe { &mut *ctx }.output_proof(
        esk,
        payment_address,
        rcm,
        value,
        unsafe { SAPLING_OUTPUT_PARAMS.as_ref() }.unwrap(),
    );

    // Write the cv out to the caller

    *unsafe { &mut *cv } = value_commitment.to_bytes();

    // Write the proof out to the caller
    proof
        .write(&mut (unsafe { &mut *zkproof })[..])
        .expect("should be able to serialize a proof");

    true
}

#[no_mangle]
pub extern "system" fn librustzcash_verify_block_signature(
    rk: *const [c_uchar; 32],
    sighash: *const [c_uchar; 32],
    sign: *const [c_uchar; 64],
) -> bool {
    // Deserialize rk
    let rk = match redjubjub::PublicKey::read(&(unsafe { &*rk })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Deserialize the signature
    let signature = match Signature::read(&(unsafe { &*sign })[..]) {
        Ok(sig) => sig,
        Err(_) => return false,
    };
    rk.verify(unsafe { &*sighash }, &signature, SPENDING_KEY_GENERATOR)
}

#[no_mangle]
pub extern "system" fn librustzcash_sign_block(
    ask: *const [c_uchar; 32],
    ar: *const [c_uchar; 32],
    sighash: *const [c_uchar; 32],
    result: *mut [c_uchar; 64],
) -> bool {
    // The caller provides the re-randomization of `ak`.
    let ar = {
        let ar = Fr::from_repr(unsafe { *ar });
        if ar.is_some().into() {
            ar.unwrap()
        } else {
            return false;
        }
    };

    // The caller provides `ask`, the spend authorizing key.
    let ask = match redjubjub::PrivateKey::read(&(unsafe { &*ask })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Initialize secure RNG
    let mut rng = OsRng;

    // Do the signing
    let rsk = ask.randomize(ar);
    let sig = rsk.sign(unsafe { &*sighash }, &mut rng, SPENDING_KEY_GENERATOR);

    // Write out the signature
    sig.write(&mut (unsafe { &mut *result })[..])
        .expect("result should be 64 bytes");

    return true;
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_spend_sig(
    ask: *const [c_uchar; 32],
    ar: *const [c_uchar; 32],
    sighash: *const [c_uchar; 32],
    result: *mut [c_uchar; 64],
) -> bool {
    // The caller provides the re-randomization of `ak`.
    let ar = {
        let ar = Fr::from_repr(unsafe { *ar });
        if ar.is_some().into() {
            ar.unwrap()
        } else {
            return false;
        }
    };

    // The caller provides `ask`, the spend authorizing key.
    let ask = match redjubjub::PrivateKey::read(&(unsafe { &*ask })[..]) {
        Ok(p) => p,
        Err(_) => return false,
    };

    // Initialize secure RNG
    let mut rng = OsRng;

    // Do the signing
    let sig = spend_sig(ask, ar, unsafe { &*sighash }, &mut rng);
    let mut bytes = [0; 64];
    // Write out the signature
    sig.write(&mut bytes.as_mut_slice())
        .expect("result should be 64 bytes");
    *unsafe { &mut *result } = bytes;
    true
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_binding_sig(
    ctx: *const SaplingProvingContext,
    value_balance: i64,
    sighash: *const [c_uchar; 32],
    result: *mut [c_uchar; 64],
) -> bool {
    let value_balance = match Amount::from_i64(value_balance) {
        Ok(vb) => vb,
        Err(()) => return false,
    };

    // Sign
    let sig = match unsafe { &*ctx }.binding_sig(value_balance, unsafe { &*sighash }) {
        Ok(s) => s,
        Err(_) => return false,
    };

    // Write out signature
    sig.write(&mut (unsafe { &mut *result })[..])
        .expect("result should be 64 bytes");

    true
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_spend_proof(
    ctx: *mut SaplingProvingContext,
    ak: *const [c_uchar; 32],
    nsk: *const [c_uchar; 32],
    diversifier: *const [c_uchar; 11],
    rcm: *const [c_uchar; 32],
    ar: *const [c_uchar; 32],
    value: u64,
    anchor: *const [c_uchar; 32],
    witness: *const [c_uchar; 1 + 33 * SAPLING_TREE_DEPTH + 8],
    cv: *mut [c_uchar; 32],
    rk_out: *mut [c_uchar; 32],
    zkproof: *mut [c_uchar; GROTH_PROOF_SIZE],
) -> bool {
    // Grab `ak` from the caller, which should be a point.
    let ak = {
        let ak = jubjub::SubgroupPoint::from_bytes(&(unsafe { *ak }));
        if ak.is_some().into() {
            ak.unwrap()
        } else {
            return false;
        }
    };

    // Grab `nsk` from the caller
    let nsk = {
        let nsk = Fr::from_repr(unsafe { *nsk });
        if nsk.is_some().into() {
            nsk.unwrap()
        } else {
            return false;
        }
    };

    // Construct the proof generation key
    let proof_generation_key = ProofGenerationKey {
        ak: ak.clone(),
        nsk,
    };

    // Grab the diversifier from the caller
    let diversifier = Diversifier(unsafe { *diversifier });

    // The caller chooses the note randomness
    let rcm = {
        let rcm = Fr::from_repr(unsafe { *rcm });
        if rcm.is_some().into() {
            Rseed::BeforeZip212(rcm.unwrap())
        } else {
            return false;
        }
    };

    // The caller also chooses the re-randomization of ak
    let ar = {
        let ar = Fr::from_repr(unsafe { *ar });
        if ar.is_some().into() {
            ar.unwrap()
        } else {
            return false;
        }
    };

    // We need to compute the anchor of the Spend.
    let anchor = {
        let anchor = Scalar::from_bytes(&unsafe { *anchor });
        if anchor.is_some().into() {
            anchor.unwrap()
        } else {
            return false;
        }
    };

    // The witness contains the incremental tree witness information, in a
    // weird serialized format.
    let witness = match MerklePath::from_slice(unsafe { &(&*witness)[..] }) {
        Ok(w) => w,
        Err(_) => return false,
    };

    // Create proof
    let (proof, value_commitment, rk) = unsafe { &mut *ctx }
        .spend_proof(
            proof_generation_key,
            diversifier,
            rcm,
            ar,
            value,
            anchor,
            witness,
            unsafe { SAPLING_SPEND_PARAMS.as_ref() }.unwrap(),
            unsafe { SAPLING_SPEND_VK.as_ref() }.unwrap(),
        )
        .expect("proving should not fail");

    // Write value commitment to caller
    *unsafe { &mut *cv } = value_commitment.to_bytes();

    // Write proof out to caller
    proof
        .write(&mut (unsafe { &mut *zkproof })[..])
        .expect("should be able to serialize a proof");

    // Write out `rk` to the caller
    rk.write(&mut unsafe { &mut *rk_out }[..])
        .expect("should be able to write to rk_out");

    true
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_proving_ctx_init() -> *mut SaplingProvingContext {
    let ctx = Box::new(SaplingProvingContext::new());

    Box::into_raw(ctx)
}

#[no_mangle]
pub extern "system" fn librustzcash_sapling_proving_ctx_free(ctx: *mut SaplingProvingContext) {
    drop(unsafe { Box::from_raw(ctx) });
}

#[no_mangle]
pub extern "system" fn librustzcash_zip32_xsk_master(
    seed: *const c_uchar,
    seedlen: size_t,
    xsk_master: *mut [c_uchar; 169],
) {
    let seed = unsafe { std::slice::from_raw_parts(seed, seedlen) };

    let xsk = zip32::ExtendedSpendingKey::master(seed);
    let mut bytes = [0u8; 169];

    xsk.write(&mut bytes.as_mut_slice())
        .expect("should be able to serialize an ExtendedSpendingKey");
    *unsafe { &mut *xsk_master } = bytes;
}

#[no_mangle]
pub extern "system" fn librustzcash_zip32_xsk_derive(
    xsk_parent: *const [c_uchar; 169],
    i: u32,
    xsk_i: *mut [c_uchar; 169],
) {
    let xsk_parent = zip32::ExtendedSpendingKey::read(&unsafe { *xsk_parent }[..])
        .expect("valid ExtendedSpendingKey");
    let i = zip32::ChildIndex::from_index(i);

    let xsk = xsk_parent.derive_child(i);
    let mut bytes = [0u8; 169];
    xsk.write(&mut bytes.as_mut_slice())
        .expect("should be able to serialize an ExtendedSpendingKey");
    *unsafe { &mut *xsk_i } = bytes;
}

#[no_mangle]
pub extern "system" fn librustzcash_zip32_xfvk_derive(
    xfvk_parent: *const [c_uchar; 169],
    i: u32,
    xfvk_i: *mut [c_uchar; 169],
) -> bool {
    let xfvk_parent = zip32::ExtendedFullViewingKey::read(&unsafe { *xfvk_parent }[..])
        .expect("valid ExtendedFullViewingKey");
    let i = zip32::ChildIndex::from_index(i);

    let xfvk = match xfvk_parent.derive_child(i) {
        Ok(xfvk) => xfvk,
        Err(_) => return false,
    };

    let mut bytes = [0u8; 169];
    xfvk.write(&mut bytes.as_mut_slice())
        .expect("should be able to serialize an ExtendedFullViewingKey");
    unsafe { *xfvk_i = bytes };
    true
}

#[no_mangle]
pub extern "system" fn librustzcash_zip32_xfvk_address(
    xfvk: *const [c_uchar; 169],
    j: *const [c_uchar; 11],
    j_ret: *mut [c_uchar; 11],
    addr_ret: *mut [c_uchar; 43],
) -> bool {
    let xfvk = zip32::ExtendedFullViewingKey::read(&unsafe { *xfvk }[..])
        .expect("valid ExtendedFullViewingKey");
    let j = zip32::DiversifierIndex(unsafe { *j });

    let (diversifier_index, addr) = match xfvk.find_address(j) {
        Some(addr) => addr,
        None => return false,
    };

    let j_ret = unsafe { &mut *j_ret };
    let addr_ret = unsafe { &mut *addr_ret };

    *j_ret = diversifier_index.0;
    *addr_ret = addr.to_bytes();

    true
}
