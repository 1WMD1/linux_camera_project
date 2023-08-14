
#ifndef _PIC_MANAGER_H
#define _PIC_MANAGER_H

#include <config.h>
#include <pic_operation.h>
#include <page_manager.h>
#include <file.h>

int RegisterPicFileParser(PT_PicFileParser ptPicFileParser);


void ShowPicFmts(void);

int PicFmtsInit(void);

int JPGParserInit(void);

int BMPParserInit(void);

PT_PicFileParser Parser(char *pcName);

PT_PicFileParser GetParser(PT_FileMap ptFileMap);

#endif /* _PIC_MANAGER_H */

