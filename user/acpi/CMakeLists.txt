###############################################################################
# ACPI Handler
#
# Contains the ACPIA code and reads ACPI tables to determine what hardware is
# available, and what other drivers to initialize.
###############################################################################
add_executable(acpisrv
    src/main.cpp
    src/Log.cpp
    src/AcpicaWrapper.cpp
    # ACPICA OS layer
    src/osl/init.cpp
    src/osl/irq.cpp
    src/osl/memory.cpp
    src/osl/mutex.cpp
    src/osl/output.cpp
    src/osl/pci.cpp
    src/osl/ports.cpp
    src/osl/power.cpp
    src/osl/semaphore.cpp
    src/osl/spinlock.cpp
    src/osl/tables.cpp
    src/osl/thread.cpp
    src/osl/time.cpp
)

target_include_directories(acpisrv PRIVATE src/acpica/include)

#### ACPICA sources
add_library(acpisrv_acpica OBJECT
    src/acpica/dispatcher/dsargs.c
    src/acpica/dispatcher/dscontrol.c
    src/acpica/dispatcher/dsdebug.c
    src/acpica/dispatcher/dsfield.c
    src/acpica/dispatcher/dsinit.c
    src/acpica/dispatcher/dsmethod.c
    src/acpica/dispatcher/dsmthdat.c
    src/acpica/dispatcher/dsobject.c
    src/acpica/dispatcher/dsopcode.c
    src/acpica/dispatcher/dspkginit.c
    src/acpica/dispatcher/dsutils.c
    src/acpica/dispatcher/dswexec.c
    src/acpica/dispatcher/dswload.c
    src/acpica/dispatcher/dswload2.c
    src/acpica/dispatcher/dswscope.c
    src/acpica/dispatcher/dswstate.c
    src/acpica/events/evxfregn.c
    src/acpica/events/evxfgpe.c
    src/acpica/events/evxfevnt.c
    src/acpica/events/evsci.c
    src/acpica/events/evxface.c
    src/acpica/events/evregion.c
    src/acpica/events/evrgnini.c
    src/acpica/events/evhandler.c
    src/acpica/events/evmisc.c
    src/acpica/events/evglock.c
    src/acpica/events/evgpe.c
    src/acpica/events/evgpeblk.c
    src/acpica/events/evgpeinit.c
    src/acpica/events/evgpeutil.c
    src/acpica/events/evevent.c
    src/acpica/executer/exresop.c
    src/acpica/executer/exconcat.c
    src/acpica/executer/exconfig.c
    src/acpica/executer/exconvrt.c
    src/acpica/executer/excreate.c
    src/acpica/executer/exdebug.c
    src/acpica/executer/exdump.c
    src/acpica/executer/exfield.c
    src/acpica/executer/exfldio.c
    src/acpica/executer/exmisc.c
    src/acpica/executer/exmutex.c
    src/acpica/executer/exnames.c
    src/acpica/executer/exoparg1.c
    src/acpica/executer/exoparg2.c
    src/acpica/executer/exoparg3.c
    src/acpica/executer/exoparg6.c
    src/acpica/executer/exprep.c
    src/acpica/executer/exregion.c
    src/acpica/executer/exresnte.c
    src/acpica/executer/exresolv.c
    src/acpica/executer/exserial.c
    src/acpica/executer/exstore.c
    src/acpica/executer/exstoren.c
    src/acpica/executer/exstorob.c
    src/acpica/executer/exsystem.c
    src/acpica/executer/extrace.c
    src/acpica/executer/exutils.c
    src/acpica/hardware/hwpci.c
    src/acpica/hardware/hwacpi.c
    src/acpica/hardware/hwesleep.c
    src/acpica/hardware/hwgpe.c
    src/acpica/hardware/hwregs.c
    src/acpica/hardware/hwsleep.c
    src/acpica/hardware/hwtimer.c
    src/acpica/hardware/hwvalid.c
    src/acpica/hardware/hwxface.c
    src/acpica/hardware/hwxfsleep.c
    src/acpica/namespace/nsdumpdv.c
    src/acpica/namespace/nsaccess.c
    src/acpica/namespace/nsalloc.c
    src/acpica/namespace/nsarguments.c
    src/acpica/namespace/nsconvert.c
    src/acpica/namespace/nsdump.c
    src/acpica/namespace/nseval.c
    src/acpica/namespace/nsinit.c
    src/acpica/namespace/nsload.c
    src/acpica/namespace/nsnames.c
    src/acpica/namespace/nsobject.c
    src/acpica/namespace/nsparse.c
    src/acpica/namespace/nspredef.c
    src/acpica/namespace/nsprepkg.c
    src/acpica/namespace/nsrepair.c
    src/acpica/namespace/nsrepair2.c
    src/acpica/namespace/nssearch.c
    src/acpica/namespace/nsutils.c
    src/acpica/namespace/nswalk.c
    src/acpica/namespace/nsxfeval.c
    src/acpica/namespace/nsxfname.c
    src/acpica/namespace/nsxfobj.c
    src/acpica/parser/psutils.c
    src/acpica/parser/psargs.c
    src/acpica/parser/psloop.c
    src/acpica/parser/psobject.c
    src/acpica/parser/psopcode.c
    src/acpica/parser/psopinfo.c
    src/acpica/parser/psparse.c
    src/acpica/parser/psscope.c
    src/acpica/parser/pstree.c
    src/acpica/parser/pswalk.c
    src/acpica/parser/psxface.c
    src/acpica/resources/rsdumpinfo.c
    src/acpica/resources/rsaddr.c
    src/acpica/resources/rscalc.c
    src/acpica/resources/rscreate.c
    #    src/acpica/resources/rsdump.c
    src/acpica/resources/rsinfo.c
    src/acpica/resources/rsio.c
    src/acpica/resources/rsirq.c
    src/acpica/resources/rslist.c
    src/acpica/resources/rsmemory.c
    src/acpica/resources/rsmisc.c
    src/acpica/resources/rsserial.c
    src/acpica/resources/rsutils.c
    src/acpica/resources/rsxface.c
    src/acpica/tables/tbxfload.c
    src/acpica/tables/tbdata.c
    src/acpica/tables/tbfadt.c
    src/acpica/tables/tbfind.c
    src/acpica/tables/tbinstal.c
    src/acpica/tables/tbprint.c
    src/acpica/tables/tbutils.c
    src/acpica/tables/tbxface.c
    src/acpica/tables/tbxfroot.c
    src/acpica/utilities/utcopy.c
    src/acpica/utilities/utaddress.c
    src/acpica/utilities/utalloc.c
    src/acpica/utilities/utascii.c
    src/acpica/utilities/utbuffer.c
    src/acpica/utilities/utcache.c
    src/acpica/utilities/utclib.c
    src/acpica/utilities/utdebug.c
    src/acpica/utilities/utdecode.c
    src/acpica/utilities/utdelete.c
    src/acpica/utilities/uterror.c
    src/acpica/utilities/uteval.c
    src/acpica/utilities/utexcep.c
    src/acpica/utilities/utglobal.c
    src/acpica/utilities/uthex.c
    src/acpica/utilities/utids.c
    src/acpica/utilities/utinit.c
    src/acpica/utilities/utlock.c
    src/acpica/utilities/utmath.c
    src/acpica/utilities/utmisc.c
    src/acpica/utilities/utmutex.c
    src/acpica/utilities/utnonansi.c
    src/acpica/utilities/utobject.c
    src/acpica/utilities/utosi.c
    src/acpica/utilities/utownerid.c
    src/acpica/utilities/utpredef.c
    #src/acpica/utilities/utprint.c
    src/acpica/utilities/utresdecode.c
    src/acpica/utilities/utresrc.c
    src/acpica/utilities/utstate.c
    src/acpica/utilities/utstring.c
    src/acpica/utilities/utstrsuppt.c
    src/acpica/utilities/utstrtoul64.c
    src/acpica/utilities/uttrack.c
    src/acpica/utilities/utuuid.c
    src/acpica/utilities/utxface.c
    src/acpica/utilities/utxferror.c
    src/acpica/utilities/utxfinit.c
    src/acpica/utilities/utxfmutex.c
)
target_compile_options(acpisrv_acpica PRIVATE -Wno-incompatible-pointer-types-discards-qualifiers
    -Wno-cast-qual)
target_include_directories(acpisrv_acpica PRIVATE src/acpica/include)
target_link_libraries(acpisrv PRIVATE acpisrv_acpica)

#####
# allow link time optimization
target_compile_options(acpisrv PRIVATE "-flto")

# search our codebase for includes
target_include_directories(acpisrv PRIVATE include)
target_include_directories(acpisrv PRIVATE src)

# required libraries
add_dependencies(acpisrv c_dynamic system_dynamic)
target_link_libraries(acpisrv PRIVATE rpc fmt::fmt-header-only)

# install it to boot directory in sysroot
install(TARGETS acpisrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

