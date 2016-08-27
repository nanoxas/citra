set(SRCS
            arm/disassembler/arm_disasm.cpp
            arm/disassembler/load_symbol_map.cpp
            arm/dyncom/arm_dyncom.cpp
            arm/dyncom/arm_dyncom_dec.cpp
            arm/dyncom/arm_dyncom_interpreter.cpp
            arm/dyncom/arm_dyncom_thumb.cpp
            arm/dyncom/arm_dyncom_trans.cpp
            arm/skyeye_common/armstate.cpp
            arm/skyeye_common/armsupp.cpp
            arm/skyeye_common/vfp/vfp.cpp
            arm/skyeye_common/vfp/vfpdouble.cpp
            arm/skyeye_common/vfp/vfpinstr.cpp
            arm/skyeye_common/vfp/vfpsingle.cpp
            core.cpp
            core_timing.cpp
            file_sys/archive_backend.cpp
            file_sys/archive_extsavedata.cpp
            file_sys/archive_romfs.cpp
            file_sys/archive_savedata.cpp
            file_sys/archive_savedatacheck.cpp
            file_sys/archive_sdmc.cpp
            file_sys/archive_systemsavedata.cpp
            file_sys/disk_archive.cpp
            file_sys/ivfc_archive.cpp
            gdbstub/gdbstub.cpp
            hle/config_mem.cpp
            hle/hle.cpp
            hle/applets/applet.cpp
            hle/applets/erreula.cpp
            hle/applets/mii_selector.cpp
            hle/applets/swkbd.cpp
            hle/kernel/address_arbiter.cpp
            hle/kernel/client_port.cpp
            hle/kernel/event.cpp
            hle/kernel/kernel.cpp
            hle/kernel/memory.cpp
            hle/kernel/mutex.cpp
            hle/kernel/process.cpp
            hle/kernel/resource_limit.cpp
            hle/kernel/semaphore.cpp
            hle/kernel/server_port.cpp
            hle/kernel/session.cpp
            hle/kernel/shared_memory.cpp
            hle/kernel/thread.cpp
            hle/kernel/timer.cpp
            hle/kernel/vm_manager.cpp
            hle/service/ac_u.cpp
            hle/service/act_a.cpp
            hle/service/act_u.cpp
            hle/service/am/am.cpp
            hle/service/am/am_app.cpp
            hle/service/am/am_net.cpp
            hle/service/am/am_sys.cpp
            hle/service/am/am_u.cpp
            hle/service/apt/apt.cpp
            hle/service/apt/apt_a.cpp
            hle/service/apt/apt_s.cpp
            hle/service/apt/apt_u.cpp
            hle/service/apt/bcfnt/bcfnt.cpp
            hle/service/boss/boss.cpp
            hle/service/boss/boss_p.cpp
            hle/service/boss/boss_u.cpp
            hle/service/cam/cam.cpp
            hle/service/cam/cam_c.cpp
            hle/service/cam/cam_q.cpp
            hle/service/cam/cam_s.cpp
            hle/service/cam/cam_u.cpp
            hle/service/cecd/cecd.cpp
            hle/service/cecd/cecd_s.cpp
            hle/service/cecd/cecd_u.cpp
            hle/service/cfg/cfg.cpp
            hle/service/cfg/cfg_i.cpp
            hle/service/cfg/cfg_s.cpp
            hle/service/cfg/cfg_u.cpp
            hle/service/csnd_snd.cpp
            hle/service/dlp/dlp.cpp
            hle/service/dlp/dlp_clnt.cpp
            hle/service/dlp/dlp_fkcl.cpp
            hle/service/dlp/dlp_srvr.cpp
            hle/service/dsp_dsp.cpp
            hle/service/err_f.cpp
            hle/service/frd/frd.cpp
            hle/service/frd/frd_a.cpp
            hle/service/frd/frd_u.cpp
            hle/service/fs/archive.cpp
            hle/service/fs/fs_user.cpp
            hle/service/gsp_gpu.cpp
            hle/service/gsp_lcd.cpp
            hle/service/hid/hid.cpp
            hle/service/hid/hid_spvr.cpp
            hle/service/hid/hid_user.cpp
            hle/service/http_c.cpp
            hle/service/ir/ir.cpp
            hle/service/ir/ir_rst.cpp
            hle/service/ir/ir_u.cpp
            hle/service/ir/ir_user.cpp
            hle/service/ldr_ro.cpp
            hle/service/mic_u.cpp
            hle/service/ndm/ndm.cpp
            hle/service/ndm/ndm_u.cpp
            hle/service/news/news.cpp
            hle/service/news/news_s.cpp
            hle/service/news/news_u.cpp
            hle/service/nim/nim.cpp
            hle/service/nim/nim_aoc.cpp
            hle/service/nim/nim_s.cpp
            hle/service/nim/nim_u.cpp
            hle/service/ns_s.cpp
            hle/service/nwm_uds.cpp
            hle/service/pm_app.cpp
            hle/service/ptm/ptm.cpp
            hle/service/ptm/ptm_play.cpp
            hle/service/ptm/ptm_sysm.cpp
            hle/service/ptm/ptm_u.cpp
            hle/service/service.cpp
            hle/service/soc_u.cpp
            hle/service/srv.cpp
            hle/service/ssl_c.cpp
            hle/service/y2r_u.cpp
            hle/shared_page.cpp
            hle/svc.cpp
            hw/gpu.cpp
            hw/hw.cpp
            hw/lcd.cpp
            hw/y2r.cpp
            loader/3dsx.cpp
            loader/elf.cpp
            loader/loader.cpp
            loader/ncch.cpp
            loader/smdh.cpp
            tracer/recorder.cpp
            memory.cpp
            settings.cpp
            system.cpp
            )

set(HEADERS
            arm/arm_interface.h
            arm/disassembler/arm_disasm.h
            arm/disassembler/load_symbol_map.h
            arm/dyncom/arm_dyncom.h
            arm/dyncom/arm_dyncom_dec.h
            arm/dyncom/arm_dyncom_interpreter.h
            arm/dyncom/arm_dyncom_run.h
            arm/dyncom/arm_dyncom_thumb.h
            arm/dyncom/arm_dyncom_trans.h
            arm/skyeye_common/arm_regformat.h
            arm/skyeye_common/armstate.h
            arm/skyeye_common/armsupp.h
            arm/skyeye_common/vfp/asm_vfp.h
            arm/skyeye_common/vfp/vfp.h
            arm/skyeye_common/vfp/vfp_helper.h
            core.h
            core_timing.h
            file_sys/archive_backend.h
            file_sys/archive_extsavedata.h
            file_sys/archive_romfs.h
            file_sys/archive_savedata.h
            file_sys/archive_savedatacheck.h
            file_sys/archive_sdmc.h
            file_sys/archive_systemsavedata.h
            file_sys/directory_backend.h
            file_sys/disk_archive.h
            file_sys/file_backend.h
            file_sys/ivfc_archive.h
            gdbstub/gdbstub.h
            hle/config_mem.h
            hle/function_wrappers.h
            hle/hle.h
            hle/applets/applet.h
            hle/applets/erreula.h
            hle/applets/mii_selector.h
            hle/applets/swkbd.h
            hle/kernel/address_arbiter.h
            hle/kernel/client_port.h
            hle/kernel/event.h
            hle/kernel/kernel.h
            hle/kernel/memory.h
            hle/kernel/mutex.h
            hle/kernel/process.h
            hle/kernel/resource_limit.h
            hle/kernel/semaphore.h
            hle/kernel/server_port.h
            hle/kernel/session.h
            hle/kernel/shared_memory.h
            hle/kernel/thread.h
            hle/kernel/timer.h
            hle/kernel/vm_manager.h
            hle/result.h
            hle/service/ac_u.h
            hle/service/act_a.h
            hle/service/act_u.h
            hle/service/am/am.h
            hle/service/am/am_app.h
            hle/service/am/am_net.h
            hle/service/am/am_sys.h
            hle/service/am/am_u.h
            hle/service/apt/apt.h
            hle/service/apt/apt_a.h
            hle/service/apt/apt_s.h
            hle/service/apt/apt_u.h
            hle/service/apt/bcfnt/bcfnt.h
            hle/service/boss/boss.h
            hle/service/boss/boss_p.h
            hle/service/boss/boss_u.h
            hle/service/cam/cam.h
            hle/service/cam/cam_c.h
            hle/service/cam/cam_q.h
            hle/service/cam/cam_s.h
            hle/service/cam/cam_u.h
            hle/service/cecd/cecd.h
            hle/service/cecd/cecd_s.h
            hle/service/cecd/cecd_u.h
            hle/service/cfg/cfg.h
            hle/service/cfg/cfg_i.h
            hle/service/cfg/cfg_s.h
            hle/service/cfg/cfg_u.h
            hle/service/csnd_snd.h
            hle/service/dlp/dlp.h
            hle/service/dlp/dlp_clnt.h
            hle/service/dlp/dlp_fkcl.h
            hle/service/dlp/dlp_srvr.h
            hle/service/dsp_dsp.h
            hle/service/err_f.h
            hle/service/frd/frd.h
            hle/service/frd/frd_a.h
            hle/service/frd/frd_u.h
            hle/service/fs/archive.h
            hle/service/fs/fs_user.h
            hle/service/gsp_gpu.h
            hle/service/gsp_lcd.h
            hle/service/hid/hid.h
            hle/service/hid/hid_spvr.h
            hle/service/hid/hid_user.h
            hle/service/http_c.h
            hle/service/ir/ir.h
            hle/service/ir/ir_rst.h
            hle/service/ir/ir_u.h
            hle/service/ir/ir_user.h
            hle/service/ldr_ro.h
            hle/service/mic_u.h
            hle/service/ndm/ndm.h
            hle/service/ndm/ndm_u.h
            hle/service/news/news.h
            hle/service/news/news_s.h
            hle/service/news/news_u.h
            hle/service/nim/nim.h
            hle/service/nim/nim_aoc.h
            hle/service/nim/nim_s.h
            hle/service/nim/nim_u.h
            hle/service/ns_s.h
            hle/service/nwm_uds.h
            hle/service/pm_app.h
            hle/service/ptm/ptm.h
            hle/service/ptm/ptm_play.h
            hle/service/ptm/ptm_sysm.h
            hle/service/ptm/ptm_u.h
            hle/service/service.h
            hle/service/soc_u.h
            hle/service/srv.h
            hle/service/ssl_c.h
            hle/service/y2r_u.h
            hle/shared_page.h
            hle/svc.h
            hw/gpu.h
            hw/hw.h
            hw/lcd.h
            hw/y2r.h
            loader/3dsx.h
            loader/elf.h
            loader/loader.h
            loader/ncch.h
            loader/smdh.h
            tracer/recorder.h
            tracer/citrace.h
            memory.h
            memory_setup.h
            mmio.h
            settings.h
            system.h
            )


if (MSVC AND USE_GCC_INTERPRETER)
    # remove interpreter from the sources list since we'll turn it into an obj file
    list(REMOVE_ITEM SRCS arm/dyncom/arm_dyncom_interpreter.cpp)

    set(FILENAME arm_dyncom_interpreter)
    set(MSYS2_MINGW64_BIN_DIR "C:/msys64/mingw64/bin/")
    set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/src/core/arm/dyncom/${FILENAME}.cpp")
    set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/src/gccinterpreter/${FILENAME}")
    set(OBJCONV "${CMAKE_BINARY_DIR}/objconv/objconv.exe")

    string(REPLACE "/" "\\" MSYS2_MINGW64_BIN_DIR ${MSYS2_MINGW64_BIN_DIR})
    string(REPLACE "/" "\\" SOURCE_FILE ${SOURCE_FILE})
    string(REPLACE "/" "\\" OUTPUT_FILE ${OUTPUT_FILE})
    string(REPLACE "/" "\\" OBJCONV ${OBJCONV})

    # add prebuild step to compile interpreter with gcc
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}.obj
        MAIN_DEPENDENCY ${SOURCE_FILE}
        COMMAND echo *** Rebuilding interpreter with gcc ***
        # Add mingw64 to path
        COMMAND set PATH=\%PATH\%\;${MSYS2_MINGW64_BIN_DIR}
        # remove old obj files
        COMMAND del ${OUTPUT_FILE}.o ${OUTPUT_FILE}.obj 2>NUL
        COMMAND ${MSYS2_MINGW64_BIN_DIR}g++ -c -static -std=gnu++14 -O3 -I${CMAKE_SOURCE_DIR}/src -I${CMAKE_SOURCE_DIR}/externals/microprofile ${SOURCE_FILE} -o ${OUTPUT_FILE}.o
        COMMAND ${OBJCONV} -fcoff64 ${OUTPUT_FILE}.o ${OUTPUT_FILE}.obj
    )
    # setup msvc to link the new obj file
    add_custom_target(gccinterpreter DEPENDS ${OUTPUT_FILE}.obj)
    set_source_files_properties(
        ${OUTPUT_FILE}.obj PROPERTIES
        EXTERNAL_OBJECT TRUE  # Identifies this as an object file
        GENERATED TRUE  # Avoids need for file to exist at configure-time
    )

    string(REPLACE "\\" "/" OUTPUT_FILE ${OUTPUT_FILE})
    create_directory_groups(${SRCS} ${HEADERS})
    add_library(core STATIC ${SRCS} ${HEADERS} ${OUTPUT_FILE}.obj)

    message("Target gccinterpreter is setup")
    unset(MSYS2_MINGW64_BIN_DIR)
    unset(CPP_FILE)
    unset(SOURCE_FILE)
    unset(OUTPUT_FILE)
    unset(OBJCONV)
endif()


if (NOT(MSVC AND USE_GCC_INTERPRETER))
    add_library(core STATIC ${SRCS} ${HEADERS})
endif()
