/*
 ============================================================================
 Name        : main.c
 Author      : sirius
 Copyright   : published under GPL
 Description : EPK2 firmware extractor for LG Electronic digital TVs
 ============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
#ifdef __CYGWIN__
#    include <sys/cygwin.h>
#endif

#include "config.h"
#include "mfile.h"

#include "epk1.h"		/* EPK v1 */
#include "epk2.h"		/* EPK v2 and v3 */
#include "hisense.h"	/* Hisense DTV */
#include "cramfs/cramfs.h"	/* CRAMFS */
#include "cramfs/cramfsswap.h"
#include "lz4/lz4.h"	/* LZ4 */
#include "lzo/lzo.h"	/* LZO */
#include "lzhs/lzhs.h"	/* LZHS */
#include "jffs2/jffs2.h"	/* JFFS2 */
#include "squashfs/unsquashfs.h"	/* SQUASHFS */
#include "minigzip.h"	/* GZIP */
#include "symfile.h"	/* SYM */
#include "tsfile.h"		/* STR and PIF */
#include "mediatek.h"	/* MTK Boot */
#include "u-boot/partinfo.h"	/* PARTINFO */
#include "util.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

struct config_opts_t config_opts;

int handle_file(char *file, struct config_opts_t *config_opts) {
	char *dest_dir = config_opts->dest_dir;
	char *file_name = my_basename(file);
	
	char *file_base = remove_ext(file_name);
	//const char *file_ext = get_ext(strdup(file_name));
	char *dest_file = NULL;

	int result = EXIT_SUCCESS;

	MFILE *mf = NULL;
	if (isFileEPK1(file)) {
		extract_epk1_file(file, config_opts);
	} else if (isFileEPK2(file)) {
		extractEPK2file(file, config_opts);
	} else if(isFileEPK3(file)) {
		extractEPK3file(file, config_opts);
	} else if((mf=is_hisense(file))){
		extract_hisense(mf, config_opts);
	} else if((mf=is_ext4_lzhs(file))){
		asprintf(&dest_file, "%s/%s.ext4", dest_dir, file_name);
		extract_ext4_lzhs(mf, dest_file);
	/* LZ4 */
	} else if ((mf=is_lz4(file))) {
		asprintf(&dest_file, "%s/%s.unlz4", dest_dir, file_name);
		printf("UnLZ4 file to: %s\n", dest_file);
		if (!LZ4_decode_file(file, dest_file))
			handle_file(dest_file, config_opts);
	/* LZO */
	} else if (check_lzo_header(file)) {
		if (!strcmp(file_name, "logo.pak"))
			asprintf(&dest_file, "%s/%s.bmp", dest_dir, file_name);
		else
			asprintf(&dest_file, "%s/%s.unlzo", dest_dir, file_name);
		printf("UnLZO file to: %s\n", dest_file);
		if (!lzo_unpack(file, dest_file))
			handle_file(dest_file, config_opts);
	/* NFSB */
	} else if ((mf=is_nfsb(file))) {
		asprintf(&dest_file, "%s/%s.unnfsb", dest_dir, file_name);
		printf("UnNFSB file to: %s\n", dest_file);
		unnfsb(file, dest_file);
		handle_file(dest_file, config_opts);
	/* SQUASHFS */
	} else if (is_squashfs(file)) {
		asprintf(&dest_file, "%s/%s.unsquashfs", dest_dir, file_name);
		printf("UnSQUASHFS file to: %s\n", dest_file);
		rmrf(dest_file);
		unsquashfs(file, dest_file);
	/* GZIP */
	} else if (is_gzip(file)) {
		asprintf(&dest_file, "%s/", dest_dir);
		printf("UnGZIP %s to folder %s\n", file, dest_file);
		strcpy(dest_file, file_uncompress_origname(file, dest_file));
		handle_file(dest_file, config_opts);
	/* MTK boot partition */
	} else if ((mf=is_mtk_boot(file))) {
		asprintf(&dest_file, "%s/mtk_1bl.bin", dest_dir);

		printf("[MTK] Extracting 1BL to mtk_1bl.bin...\n");
		extract_mtk_1bl(mf, dest_file);

		printf("[MTK] Extracting embedded LZHS files...\n");
		extract_lzhs(mf);
	/* CRAMFS Big Endian */
	} else if (is_cramfs_image(file, "be")) {
		asprintf(&dest_file, "%s/%s.cramswap", dest_dir, file_name);
		printf("Swapping cramfs endian for file %s\n", file);
		cramswap(file, dest_file);
		handle_file(dest_file, config_opts);
	/* CRAMFS Little Endian */
	} else if (is_cramfs_image(file, "le")) {
		asprintf(&dest_file, "%s/%s.uncramfs", dest_dir, file_name);
		printf("UnCRAMFS %s to folder %s\n", file, dest_file);
		rmrf(dest_file);
		uncramfs(dest_file, file);
	/* Kernel uImage */
	} else if (is_kernel(file)) {
		asprintf(&dest_file, "%s/%s.unpaked", dest_dir, file_name);
		printf("Extracting boot image (kernel) to: %s\n", dest_file);
		extract_kernel(file, dest_file);
		handle_file(dest_file, config_opts);
	/* Partition Table (partinfo) */
	} else if (isPartPakfile(file)) {
		asprintf(&dest_file, "%s/%s.txt", dest_dir, file_base);
		printf("Saving partition info to: %s\n", dest_file);
		dump_partinfo(file, dest_file);
	/* JFFS2 */
	} else if (is_jffs2(file)) {
		asprintf(&dest_file, "%s/%s.unjffs2", dest_dir, file_name);
		printf("UnJFFS2 file %s to folder %s\n", file, dest_file);
		rmrf(dest_file);
		jffs2extract(file, dest_file, "1234");
	/* PVR STR (ts/m2ts video) */
	} else if (isSTRfile(file)) {
		asprintf(&dest_file, "%s/%s.ts", dest_dir, file_name);
		setKey();
		printf("\nConverting %s file to TS: %s\n", file, dest_file);
		convertSTR2TS(file, dest_file, 0);
	/* PVR PIF (Program Information File) */ 
	} else if (!memcmp(&file[strlen(file) - 3], "PIF", 3)) {
		asprintf(&dest_file, "%s/%s.ts", dest_dir, file_name);
		setKey();
		printf("\nProcessing PIF file: %s\n", file);
		processPIF(file, dest_file);
	/* SYM File (Debugging information) */
	} else if (symfile_load(file) == 0) {
		asprintf(&dest_file, "%s/%s.idc", dest_dir, file_name);
		printf("Converting SYM file to IDC script: %s\n", dest_file);
		symfile_write_idc(dest_file);
	/* MTK LZHS (Modified LZSS + Huffman) */
	} else if ((mf=is_lzhs(file))) {
		asprintf(&dest_file, "%s/%s.unlzhs", dest_dir, file_name);
		printf("UnLZHS %s to %s\n", file, dest_file);
		lzhs_decode(mf, 0, dest_file, NULL);
	/* MTK TZFW (TrustZone Firmware) */
	} else if (!strcmp(file_name, "tzfw.pak") && (mf=is_elf(file))) {
		printf("Splitting mtk tzfw...\n");
		split_mtk_tz(mf, dest_dir);
	} else {
		result = EXIT_FAILURE;
	}
	
	if(mf != NULL)
		mclose(mf);

	free(file_name);
	free(file_base);
	
	if(dest_file != NULL)
		free(dest_file);
	
	return result;
}

int main(int argc, char *argv[]) {
	printf("\nLG Electronics digital TV firmware package (EPK) extractor version 4.4 by sirius (http://openlgtv.org.ru)\n\n");
	if (argc < 2) {
		printf("Thanks to xeros, tbage, jenya, Arno1, rtokarev, cronix, lprot, Smx and all other guys from openlgtv project for their kind assistance.\n\n");
		printf("Usage: epk2extract [-options] FILENAME\n\n");
		printf("Options:\n");
		printf("  -c : extract to current directory instead of source file directory\n\n");
		return err_ret("");
	}

	char *exe_dir = calloc(1, PATH_MAX);
	char *current_dir = calloc(1, PATH_MAX);
	
	#ifdef __APPLE__
	uint32_t pathsz = PATH_MAX;
	if (_NSGetExecutablePath(exe_dir, &pathsz) == 0){
		printf("Executable path is %s\n", exe_dir);
	} else {
		printf("Buffer too small; need size %u\n", PATH_MAX);
		return EXIT_FAILURE;
	}
	config_opts.config_dir = my_dirname(exe_dir);
	#else
	getcwd(current_dir, PATH_MAX);
	printf("Current directory: %s\n", current_dir);
	readlink("/proc/self/exe", exe_dir, PATH_MAX);
	#endif
	
	config_opts.config_dir = my_dirname(exe_dir);
	config_opts.dest_dir = calloc(1, PATH_MAX);

	int opt;
	while ((opt = getopt(argc, argv, "c")) != -1) {
		switch (opt) {
		case 'c':{
				strcpy(config_opts.dest_dir, current_dir);
				break;
			}
		case ':':{
				printf("Option `%c' needs a value\n\n", optopt);
				exit(1);
				break;
			}
		case '?':{
				printf("Unknown option: `%c'\n\n", optopt);
				return 1;
			}
		}
	}

#ifdef __CYGWIN__
	char posix[PATH_MAX];
	cygwin_conv_path(CCP_WIN_A_TO_POSIX, argv[optind], posix, PATH_MAX);
	char *input_file = posix;
#else
	char *input_file = argv[optind];
#endif
	printf("Input file: %s\n", input_file);
	if (strlen(config_opts.dest_dir) == 0){
			char *dname = my_dirname(input_file);
			strcpy(config_opts.dest_dir, dname);
			free(dname);
	}
	if (strlen(config_opts.dest_dir) == 0 && config_opts.dest_dir[0] == '.'){
		char *dname = my_dirname(exe_dir);
		strcpy(config_opts.dest_dir, dname);
	}

	printf("Destination directory: %s\n", config_opts.dest_dir);
	
	free(exe_dir);
	free(current_dir);

	int exit_code = handle_file(input_file, &config_opts);
	
	if (exit_code == EXIT_FAILURE)
		return err_ret("Unsupported input file format: %s\n\n", input_file);

	return !err_ret("\nExtraction is finished.\n\n");
}
