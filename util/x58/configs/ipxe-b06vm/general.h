#ifndef CONFIG_LOCAL_B06VM_GENERAL_H
#define CONFIG_LOCAL_B06VM_GENERAL_H

/*
 * MSI X58 Pro-E B06VM: full legacy iPXE feature set plus HTTPS and image
 * trust/verification commands.  Keep iPXE's normal defaults so this named
 * profile is equivalent to coreboot's CONFIG_IPXE_HAS_HTTPS=y and
 * CONFIG_IPXE_TRUST_CMD=y integration path.
 */
#define DOWNLOAD_PROTO_HTTPS
#define IMAGE_TRUST_CMD

#endif /* CONFIG_LOCAL_B06VM_GENERAL_H */
