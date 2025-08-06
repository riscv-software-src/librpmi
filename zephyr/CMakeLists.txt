if(CONFIG_RISCV_RPMI)

zephyr_library_named(rpmi)

zephyr_library_sources(
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_context.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_hsm.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_service_group_clock.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_service_group_cppc.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_service_group_hsm.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_service_group_sysreset.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_service_group_syssusp.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_shmem.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_transport_shmem.c
  ${ZEPHYR_LIBRPMI_MODULE_DIR}/lib/rpmi_transport.c
)

  zephyr_include_directories(
    # ordering is important here, since we override the librpmi_env.h header
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZEPHYR_LIBRPMI_MODULE_DIR}/include
  )
endif()
