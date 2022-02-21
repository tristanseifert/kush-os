#ifndef KERNEL_BUILDINFO_H
#define KERNEL_BUILDINFO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Information about a kernel build
 *
 * This structure holds some metadata about this particular build of the kernel, including the
 * associated git revision and build type.
 */
struct BuildInfo {
    /// Branch from which the kernel is built
    const char * const gitBranch;
    /// Commit hash the kernel was built from
    const char * const gitHash;

    /// Architecture for which the kernel is built
    const char * const arch;
    /// Platform for which the kernel is built
    const char * const platform;

    /// Build type (specifies optimizations, etc.)
    const char * const buildType;
    /// User that built the kernel
    const char * const buildUser;
    /// Hostname of the build machine
    const char * const buildHost;
    /// Date/time at which the build took place
    const char * const buildDate;
};

/**
 * Global build info structure, containing information on the configuration of the kernel.
 */
extern const struct BuildInfo gBuildInfo;

#ifdef __cplusplus
}
#endif

#endif
