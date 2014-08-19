/**
 * ms-biff.h: MS Excel BIFF support for Gnumeric
 *
 * Author:
 *    Michael Meeks (michael@ximian.com)
 *
 * (C) 1998, 1999, 2000 Michael Meeks
 **/

#ifndef GNUMERIC_BIFF_H
#define GNUMERIC_BIFF_H

#include <gretl/plugin/libole2/ms-ole.h>

#undef EDEBUG

typedef struct _BiffQuery BiffQuery;

struct _BiffQuery {
    guint8  ms_op;
    guint8  ls_op;
    guint16 opcode;

    guint8  *data;
    int     data_malloced;
    guint32 length;

    guint32 streamPos;
    MsOleStream *pos;
};

typedef enum { 
    MS_BIFF_V2 = 2,
    MS_BIFF_V3 = 3,
    MS_BIFF_V4 = 4,
    MS_BIFF_V5 = 5, /* Excel 5.0 */
    MS_BIFF_V7 = 7, /* Excel 95 */
    MS_BIFF_V8 = 8, /* Excel 97 */
    MS_BIFF_V_UNKNOWN = 0
} MsBiffVersion;


/* Pass this a BiffQuery * */
#define EX_GETROW(p)      (MS_OLE_GET_GUINT16(p->data + 0))
#define EX_GETCOL(p)      (MS_OLE_GET_GUINT16(p->data + 2))
#define EX_GETXF(p)       (MS_OLE_GET_GUINT16(p->data + 4))
#define EX_GETSTRLEN(p)   (MS_OLE_GET_GUINT16(p->data + 6))

/* Version info types as found in various Biff records */
typedef enum { 
    MS_BIFF_TYPE_Workbook, 
    MS_BIFF_TYPE_VBModule, 
    MS_BIFF_TYPE_Worksheet,
    MS_BIFF_TYPE_Chart, 
    MS_BIFF_TYPE_Macrosheet, 
    MS_BIFF_TYPE_Workspace,
    MS_BIFF_TYPE_Unknown 
} MsBiffFileType;

/* Cell / XF types */
typedef enum { 
    MS_BIFF_H_VISIBLE, 
    MS_BIFF_H_HIDDEN,
    MS_BIFF_H_VERY_HIDDEN 
} MsBiffHidden;

typedef enum { 
    MS_BIFF_X_STYLE, 
    MS_BIFF_X_CELL 
} MsBiffXfType;

typedef enum { 
    MS_BIFF_F_MS, 
    MS_BIFF_F_LOTUS 
} MsBiffFormat;

typedef enum { 
    MS_BIFF_E_CONTEXT, 
    MS_BIFF_E_LEFT_TO_RIGHT,
    MS_BIFF_E_RIGHT_TO_LEFT 
} MsBiffEastern;

typedef struct {
    MsBiffVersion version;
    MsBiffFileType type;
} MsBiffBofData;

typedef enum {
    MsBiffMaxRowsV7 = 16384,
    MsBiffMaxRowsV8 = 65536
} MsBiffMaxRows;

int excel_book_get_info (const char *fname, wbook *book);
MsBiffBofData *new_ms_biff_bof_data  (BiffQuery *pos);
void free_ms_biff_bof_data (MsBiffBofData *data);
BiffQuery *ms_biff_query_new (MsOleStream *ptr);
int ms_biff_query_next (BiffQuery *bq);
void ms_biff_query_destroy (BiffQuery *bq);

/**
 * biff-types.h: A long and dull list of BIFF types.
 *
 * Author:
 *    Michael Meeks (michael@ximian.com)
 *
 * (C) 1998-2001 Michael Meeks
 **/

#define BIFF_DIMENSIONS                 0x00	/* 2, NOT 1,10 */
#define BIFF_BLANK                      0x01	/* 2, NOT 10 */
#define BIFF_NUMBER                     0x03	/* 2, NOT 1,10 */
#define BIFF_LABEL                      0x04	/* 2 */
#define BIFF_BOOLERR                    0x05	/* 2, NOT 10 */
#define BIFF_FORMULA                    0x06	/* 4, NOT 10 */
#define BIFF_STRING                     0x07	/* 2 */
#define BIFF_ROW                        0x08	/* 2 */
#define BIFF_BOF                        0x09	/* 8, NOT 10 */
#define BIFF_EOF                        0x0a	/* 0, NOT 10 */
#define BIFF_INDEX                      0x0b	/* 2, NOT 10 */
#define BIFF_CALCCOUNT                  0x0c	/* 0, NOT 10 */
#define BIFF_CALCMODE                   0x0d	/* 0, NOT 10 */
#define BIFF_PRECISION                  0x0e	/* 0 */
#define BIFF_REFMODE                    0x0f	/* 0 */
#define BIFF_DELTA                      0x10	/* 0 */
#define BIFF_ITERATION                  0x11	/* 0 */
#define BIFF_PROTECT                    0x12	/* 0 */
#define BIFF_PASSWORD                   0x13	/* 0 */
#define BIFF_HEADER                     0x14	/* 0, NOT 10 */
#define BIFF_FOOTER                     0x15	/* 0, NOT 10 */
#define BIFF_EXTERNCOUNT                0x16	/* 0, NOT 10 */
#define BIFF_EXTERNSHEET                0x17	/* 0, NOT 10 */
#define BIFF_NAME                       0x18	/* 2, NOT 10 */
#define BIFF_WINDOWPROTECT              0x19	/* 0, NOT 10 */
#define BIFF_VERTICALPAGEBREAKS         0x1a	/* 0, NOT 10 */
#define BIFF_HORIZONTALPAGEBREAKS       0x1b	/* 0, NOT 10 */
#define BIFF_NOTE                       0x1c	/* 0, NOT 10 */
#define BIFF_SELECTION                  0x1d	/* 0, NOT 10 */
#define BIFF_FORMAT                     0x1e	/* 4, NOT 10 */
#define BIFF_ARRAY                      0x21	/* 2, NOT 10 */
#define BIFF_1904                       0x22	/* 0, NOT 1,10 */
#define BIFF_EXTERNNAME                 0x23	/* 2 */
#define BIFF_DEFAULTROWHEIGHT		0x25	/* 2, NOT 10 */
#define BIFF_LEFT_MARGIN                0x26	/* 0, NOT 10 */
#define BIFF_RIGHT_MARGIN               0x27	/* 0, NOT 10 */
#define BIFF_TOP_MARGIN                 0x28	/* 0 */
#define BIFF_BOTTOM_MARGIN              0x29	/* 0 */
#define BIFF_PRINTHEADERS               0x2a	/* 0 */
#define BIFF_PRINTGRIDLINES             0x2b	/* 0 */
#define BIFF_FILEPASS                   0x2f	/* 0 */
#define BIFF_FONT                       0x31	/* 2 */
#define BIFF_PRINTSIZE                  0x33	/* 0, Undocumented */
#define BIFF_TABLE                      0x36	/* 2 */
#define BIFF_CONTINUE                   0x3c	/* 0, NOT 10 */
#define BIFF_WINDOW1                    0x3d	/* 0, NOT 1,10 */
#define BIFF_WINDOW2                    0x3e	/* 2, NOT 10 */
#define BIFF_BACKUP                     0x40	/* 0, NOT 10 */
#define BIFF_PANE                       0x41	/* 0, NOT 10 */
#define BIFF_CODEPAGE                   0x42	/* DUPLICATE 42 */
#define BIFF_XF_OLD                     0x43	/* What is this ?, NOT 10 */
#define BIFF_PLS                        0x4d	/* 0 */
#define BIFF_DCON                       0x50	/* 0, NOT 10 */
#define BIFF_DCONREF                    0x51	/* 0, NOT 10 */
#define BIFF_DCONNAME                   0x52	/* 0 */
#define BIFF_DEFCOLWIDTH                0x55	/* 0 */
#define BIFF_XCT                        0x59	/* 0 */
#define BIFF_CRN                        0x5a	/* 0 */
#define BIFF_FILESHARING                0x5b	/* 0, NOT 10 */
#define BIFF_WRITEACCESS                0x5c	/* 0 */
#define BIFF_OBJ                        0x5d	/* 0, NOT 10 */
#define BIFF_UNCALCED                   0x5e	/* 0 */
#define BIFF_SAVERECALC                 0x5f	/* 0 */
#define BIFF_TEMPLATE                   0x60	/* 0, NOT 1,10 */
#define BIFF_TAB_COLOR                  0x62	/* 8 */
#define BIFF_OBJPROTECT                 0x63	/* 0, NOT 10 */
#define BIFF_COLINFO                    0x7d	/* 0 */
#define BIFF_RK                         0x7e	/* 0 */
#define BIFF_IMDATA                     0x7f	/* 0 */
#define BIFF_GUTS                       0x80	/* 0 */
#define BIFF_WSBOOL                     0x81	/* 0 */
#define BIFF_GRIDSET                    0x82	/* 0 */
#define BIFF_HCENTER                    0x83	/* 0 */
#define BIFF_VCENTER                    0x84	/* 0 */
#define BIFF_BOUNDSHEET                 0x85	/* 0 */
#define BIFF_WRITEPROT                  0x86	/* 0 */
#define BIFF_ADDIN                      0x87	/* 0 */
#define BIFF_EDG                        0x88	/* 0 */
#define BIFF_PUB                        0x89	/* 0 */
#define BIFF_COUNTRY                    0x8c	/* 0 */
#define BIFF_HIDEOBJ                    0x8d	/* 0 */
#define BIFF_SORT                       0x90	/* 0 */
#define BIFF_SUB                        0x91	/* 0 */
#define BIFF_PALETTE                    0x92	/* 0 */
#define BIFF_STYLE                      0x93	/* 2 */
#define BIFF_LHRECORD                   0x94	/* 0 */
#define BIFF_LHNGRAPH                   0x95	/* 0 */
#define BIFF_SOUND                      0x96	/* 0 */
#define BIFF_LPR                        0x98	/* 0 */
#define BIFF_STANDARDWIDTH              0x99	/* 0 */
#define BIFF_FNGROUPNAME                0x9a	/* 0 */
#define BIFF_FILTERMODE                 0x9b	/* 0 */
#define BIFF_FNGROUPCOUNT               0x9c	/* 0 */
#define BIFF_AUTOFILTERINFO             0x9d	/* 0 */
#define BIFF_AUTOFILTER                 0x9e	/* 0 */
#define BIFF_SCL                        0xa0	/* 0 */
#define BIFF_SETUP                      0xa1	/* 0 */
#define BIFF_COORDLIST                  0xa9	/* 0 NOT 1 */
#define BIFF_GCW                        0xab	/* 0 NOT 1 */
#define BIFF_SCENMAN                    0xae	/* 0 NOT 1 */
#define BIFF_SCENARIO                   0xaf	/* 0 NOT 1 */
#define BIFF_SXVIEW                     0xb0	/* 0 */
#define BIFF_SXVD                       0xb1	/* 0 NOT 1 */
#define BIFF_SXVI                       0xb2	/* 0 NOT 1 */
#define BIFF_SXIVD                      0xb4	/* 0 */
#define BIFF_SXLI                       0xb5	/* 0 NOT 1 */
#define BIFF_SXPI                       0xb6	/* 0 NOT 1 */
#define BIFF_DOCROUTE                   0xb8	/* 0 NOT 1 */
#define BIFF_RECIPNAME                  0xb9	/* 0 */
#define BIFF_SHRFMLA                    0xbc	/* 0 NOT 1 */
#define BIFF_MULRK                      0xbd	/* 0 */
#define BIFF_MULBLANK                   0xbe	/* 0 NOT 1 */
#define BIFF_MMS                        0xc1	/* 0 */
#define BIFF_ADDMENU                    0xc2	/* 0 */
#define BIFF_DELMENU                    0xc3	/* 0 */
#define BIFF_SXDI                       0xc5	/* 0 */
#define BIFF_SXDB                       0xc6	/* 0 */
#define BIFF_SXSTRING                   0xcd	/* 0 */
#define BIFF_SXTBL                      0xd0	/* 0 */
#define BIFF_SXTBRGIITM                 0xd1	/* 0 */
#define BIFF_SXTBPG                     0xd2	/* 0 */
#define BIFF_OBPROJ                     0xd3	/* 0 */
#define BIFF_SXIDSTM                    0xd5	/* 0 */
#define BIFF_RSTRING                    0xd6	/* 0 */
#define BIFF_DBCELL                     0xd7	/* 0 */
#define BIFF_BOOKBOOL                   0xda	/* 0 */
#define BIFF_PARAMQRY                   0xdc	/* DUPLICATE dc */
#define BIFF_SXEXT                      0xdc	/* DUPLICATE dc */
#define BIFF_SCENPROTECT                0xdd	/* 0 */
#define BIFF_OLESIZE                    0xde	/* 0 */
#define BIFF_UDDESC                     0xdf	/* 0 */
#define BIFF_XF                         0xe0	/* 0 */
#define BIFF_INTERFACEHDR               0xe1	/* 0 */
#define BIFF_INTERFACEEND               0xe2	/* 0 */
#define BIFF_SXVX                       0xe3	/* 0 */
#define BIFF_MERGECELLS                 0xe5	/* Undocumented */
#define BIFF_BG_PIC                 	0xe9	/* Undocumented */
#define BIFF_TABIDCONF                  0xea	/* 0 */
#define BIFF_MS_O_DRAWING_GROUP         0xeb	/* 0 */
#define BIFF_MS_O_DRAWING               0xec	/* 0 */
#define BIFF_MS_O_DRAWING_SELECTION     0xed	/* 0 */
#define BIFF_PHONETIC			0xef	/* semi-Undocumented */
#define BIFF_SXRULE                     0xf0	/* 0 */
#define BIFF_SXEX                       0xf1	/* 0 */
#define BIFF_SXFILT                     0xf2	/* 0 */
#define BIFF_SXNAME                     0xf6	/* 0 */
#define BIFF_SXSELECT                   0xf7	/* 0 */
#define BIFF_SXPAIR                     0xf8	/* 0 */
#define BIFF_SXFMLA                     0xf9	/* 0 */
#define BIFF_SXFORMAT                   0xfb	/* 0 */
#define BIFF_SST                        0xfc	/* 0 */
#define BIFF_LABELSST                   0xfd	/* 0 */
#define BIFF_EXTSST                     0xff	/* 0 */

/* Odd balls */
#define BIFF_SXVDEX                    0x100	/* ONLY 1 */
#define BIFF_SXFORMULA                 0x103	/* ONLY 1 */
#define BIFF_SXDBEX                    0x122	/* ONLY 1 */
#define BIFF_TABID                     0x13d	/* ONLY 1 */
#define BIFF_USESELFS                  0x160	/* ONLY 1 */
#define BIFF_DSF                       0x161	/* ONLY 1 */
#define BIFF_XL5MODIFY                 0x162	/* ONLY 1 */
#define BIFF_FILESHARING2              0x1a5
#define BIFF_USERDBVIEW                0x1a9	/* ONLY 1 */
#define BIFF_USERSVIEWBEGIN            0x1aa
#define BIFF_USERSVIEWEND              0x1ab	/* ONLY 1 */
#define BIFF_QSI                       0x1ad
#define BIFF_SUPBOOK                   0x1ae	/* ONLY 1 */
#define BIFF_PROT4REV                  0x1af	/* ONLY 1 */
#define BIFF_CONDFMT                   0x1b0	/* ONLY 1 */
#define BIFF_CF                        0x1b1	/* ONLY 1 */
#define BIFF_DVAL                      0x1b2	/* ONLY 1 */
#define BIFF_DCONBIN                   0x1b5	/* ONLY 1 */
#define BIFF_TXO                       0x1b6	/* ONLY 1 */
#define BIFF_REFRESHALL                0x1b7	/* ONLY 1 */
#define BIFF_HLINK                     0x1b8	/* ONLY 1 */
#define BIFF_CODENAME                  0x1ba	/* ONLY 1, TYPO in MS Docs */
#define BIFF_SXFDBTYPE                 0x1bb
#define BIFF_PROT4REVPASS              0x1bc	/* ONLY 1 */
#define BIFF_DV                        0x1be	/* ONLY 1 */
#define BIFF_XL9FILE		       0x1c0	/* ONLY 1 */
#define BIFF_RECALCID		       0x1c1	/* ONLY 1 */
#define BIFF_UNKNOWN_1		       0x810	/* what this is */

#endif /* GNUMERIC_BIFF_H */
