#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mfile.h"
#include "lzhs/lzhs.h"
#include "mediatek.h"
#include "util.h"

void scan_lzhs(const char *filename, int extract) {
	int fsize, pos;
	const int  FIRST_PART_SIZE = 0x100000;

	struct lzhs_header *header = NULL;
	int header_size = sizeof(*header);
	uint32_t compressedSize;
	char *outname, *outdecode;

	MFILE *file = mopen(filename, O_RDONLY);
	if (file == NULL) {
		printf("Can't open file %s\n", filename);
		exit(1);
	}

	if (extract) {
		char *dirn = my_dirname(filename);
		char *filen = my_basename(filename);

		asprintf(&outname, "%s/%s_file%03d.unlzhs", dirn, filen, 0);

		MFILE *out = mfopen(outname, "w+");
		if (out == NULL) {
			err_exit("Cannot open file %s for writing\n", outname);
		}

		mfile_map(out, FIRST_PART_SIZE);

		memcpy(
			mdata(out, void),
			mdata(file, void),
			FIRST_PART_SIZE
		);

		free(outname);
		mclose(out);
		free(dirn);
		free(filen);
	}

	int i, count = 0, count_internal;
	for (i = FIRST_PART_SIZE + header_size; i<msize(file); i += header_size) {
		header = (struct lzhs_header *)(mdata(file, uint8_t) + i - header_size );
		if (_is_lzhs_mem(header)) {
			count_internal = header->checksum;	// first LZHS header contain number of block in checksum
			compressedSize = header->compressedSize;
			header = (struct lzhs_header *)(mdata(file, uint8_t) + i);
			if (_is_lzhs_mem(header) && compressedSize == (header->compressedSize + header_size)) {
				count++;

				printf("---------------------------------------------------\n");
				off_t fileOff = moff(file, header);
				if(!(fileOff % MTK_LOADER_OFF)){
					printf("Found possible mtk loader at 0x%x\n", fileOff);
				} else if(!(fileOff % MTK_UBOOT_OFF)){
					printf("Found possible mtk uboot at 0x%x\n", fileOff);
				} else {
					printf("Found possible LZHS header at 0x%x\n", fileOff);
				}

				if (extract) {
					char *dirn = my_dirname(filename);
					char *filen = my_basename(filename);

					asprintf(&outname, "%s/%s_file%03d.lzhs", dirn, filen, count_internal);

					printf("Extracting to %s\n", outname);

					MFILE *out = mfopen(outname, "w+");
					if (out == NULL) {
						err_exit("Cannot open file %s for writing\n", outname);
					}
				
					mfile_map(out, sizeof(*header) + header->compressedSize);
				
					memcpy(
						mdata(out, void),
						(uint8_t *)header,
						sizeof(*header) + header->compressedSize
					);
					
					free(outname);

					asprintf(&outdecode, "%s/%s_file%03d.unlzhs", dirn, filen, count_internal);
					lzhs_decode(out, outdecode);
				
					mclose(out);
				
					free(outdecode);
					free(dirn);
					free(filen);
				}
			}
		}
	}
	mclose(file);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: \n");
		printf("'%s [in] 0' scan\n", argv[0]);
		printf("'%s [in] 1' scan and extract\n", argv[0]);
		return 1;
	}
	scan_lzhs(argv[1], atoi(argv[2]));
	return 0;
}
