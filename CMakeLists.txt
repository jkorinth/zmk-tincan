if(CONFIG_ZMK_TINCAN)
	zephyr_library()
	zephyr_library_sources(src/tincan.c)
endif()
