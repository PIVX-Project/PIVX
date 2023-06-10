package=native_rust
$(package)_version=1.57.0
$(package)_download_path=https://depends.pivx.org
$(package)_file_name_x86_64_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.xz
$(package)_sha256_hash_x86_64_linux=62b209684cdabda28fa82c4422283cf77f8dad8958c6659de107bf32955b12eb
$(package)_file_name_arm_linux=rust-$($(package)_version)-arm-unknown-linux-gnueabihf.tar.xz
$(package)_sha256_hash_arm_linux=aa9a52bcf7f8a2da805305aaf831b3b241b4c1465dba328ac782c51ff45d5379
$(package)_file_name_armv7l_linux=rust-$($(package)_version)-armv7-unknown-linux-gnueabihf.tar.xz
$(package)_sha256_hash_armv7l_linux=aa9a52bcf7f8a2da805305aaf831b3b241b4c1465dba328ac782c51ff45d5379
$(package)_file_name_aarch64_linux=rust-$($(package)_version)-aarch64-unknown-linux-gnu.tar.xz
$(package)_sha256_hash_aarch64_linux=589edb5d934f4abc4f38b0b6ee2b8f634385f84fc021da6306fb1020823e52ae
$(package)_file_name_x86_64_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_sha256_hash_x86_64_darwin=10d92d8c3019a9d7eca5186594499e4fb6bbaab13c98966bacaf760a776b5873
$(package)_file_name_aarch64_darwin=rust-$($(package)_version)-aarch64-apple-darwin.tar.xz
$(package)_sha256_hash_aarch64_darwin=74f23430ef168746d733c2c30c41cb4e3d13946106945bc4ad94eaa67dc73db0
$(package)_file_name_x86_64_freebsd=rust-$($(package)_version)-x86_64-unknown-freebsd.tar.xz
$(package)_sha256_hash_x86_64_freebsd=b9984cc15c3158c0b41e8564564102eb7e8b261c1e59ed4dca87fdbdf551a0a5

# Mapping from GCC canonical hosts to Rust targets
# If a mapping is not present, we assume they are identical, unless $host_os is
# "darwin", in which case we assume x86_64-apple-darwin.
$(package)_rust_target_x86_64-w64-mingw32=x86_64-pc-windows-gnu
$(package)_rust_target_i686-pc-linux-gnu=i686-unknown-linux-gnu
$(package)_rust_target_riscv64-linux-gnu=riscv64gc-unknown-linux-gnu
$(package)_rust_target_riscv64-unknown-linux-gnu=riscv64gc-unknown-linux-gnu
$(package)_rust_target_x86_64-linux-gnu=x86_64-unknown-linux-gnu
$(package)_rust_target_x86_64-pc-linux-gnu=x86_64-unknown-linux-gnu
$(package)_rust_target_armv7l-unknown-linux-gnueabihf=arm-unknown-linux-gnueabihf

# Mapping from Rust targets to SHA-256 hashes
$(package)_rust_std_sha256_hash_arm-unknown-linux-gnueabihf=5b7bb65226d295e1b357306df1df9bf0f5c026a1a059e198066889de69dd0f70
$(package)_rust_std_sha256_hash_aarch64-unknown-linux-gnu=6613099e77d0bce9b672a7f4a5d83f2529f0834b6e1fc0d87d1cff4c54a6e1cf
$(package)_rust_std_sha256_hash_i686-unknown-linux-gnu=36c114d412e2b7ee9437bb42f7e315b4b249a56d09fb2eef1bbde8242479a700
$(package)_rust_std_sha256_hash_x86_64-unknown-linux-gnu=38bf66c9677a34396f59287dee0873c7fc9099a4b2039643897a36d437793be2
$(package)_rust_std_sha256_hash_riscv64gc-unknown-linux-gnu=5deee091cd50847b2442481a1136f2c6bb590696c72a6abdda5ae333ad84af95
$(package)_rust_std_sha256_hash_x86_64-apple-darwin=46c2c8fd6aeffbfb62bc03ba913d65b37dfdb328405e2b75a540b6f68e1031c2
$(package)_rust_std_sha256_hash_aarch64-apple-darwin=1636ae0aa61c0d64bbcaf424d0a18b4f1e604b538ffdf86d710beeb1e36ffbf4
$(package)_rust_std_sha256_hash_x86_64-pc-windows-gnu=478b3cd0ade0076998c72b84d2b74c6ec94b6ebbf37630bbd30803deb8651fc8

define rust_target
$(if $($(1)_rust_target_$(2)),$($(1)_rust_target_$(2)),$(if $(findstring darwin,$(3)),$(if $(findstring aarch64,$(host_arch)),aarch64-apple-darwin,x86_64-apple-darwin),$(2)))
endef

ifneq ($(canonical_host),$(build))
$(package)_rust_target=$(call rust_target,$(package),$(canonical_host),$(host_os))
$(package)_exact_file_name=rust-std-$($(package)_version)-$($(package)_rust_target).tar.xz
$(package)_exact_sha256_hash=$($(package)_rust_std_sha256_hash_$($(package)_rust_target))
$(package)_build_subdir=buildos
$(package)_extra_sources=$($(package)_file_name_$(build_arch)_$(build_os))

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_sha256_hash_$(build_arch)_$(build_os)))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_sha256_hash_$(build_arch)_$(build_os))  $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os))" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir $(canonical_host) && \
  tar --strip-components=1 -xf $($(package)_source) -C $(canonical_host) && \
  mkdir buildos && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os)) -C buildos
endef

define $(package)_stage_cmds
  bash ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) --disable-ldconfig && \
  ../$(canonical_host)/install.sh --without=rust-docs --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) --disable-ldconfig
endef
else

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_file_name_$(build_arch)_$(build_os)),$($(package)_sha256_hash_$(build_arch)_$(build_os)))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash_$(build_arch)_$(build_os))  $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os))" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir $(canonical_host) && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_file_name_$(build_arch)_$(build_os)) -C $(canonical_host)
endef

define $(package)_stage_cmds
  bash ./$(canonical_host)/install.sh --without=rust-docs --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) --disable-ldconfig
endef
endif
