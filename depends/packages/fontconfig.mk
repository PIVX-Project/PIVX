package=fontconfig
$(package)_version=2.12.5
$(package)_download_path=http://www.freedesktop.org/software/fontconfig/release/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=e10ccf1e26b0968f61d81037af1147fea28e86bfd159ffd8cfd5a486da126ce4
$(package)_dependencies=freetype expat

define $(package)_set_vars
  $(package)_config_opts=--disable-docs --disable-static
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
