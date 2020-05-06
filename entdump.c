// From http://old.r1ch.net/stuff/r1q2/ - thanks R1ch.
//GPL etc.
//
// entdump is used for extracting entities from quake2 bsp files in text
// format for usage with the added ent file support in Xatrix+
//
// Build like: gcc -o entdump entdump.c
// Use like:  # ./entdump map.bsp > map.ent
// Nick I fixed so that the ent.file didn't have a double newline at the end.

// January 11, 2010, QwazyWabbit added texture file name output, usage info
// July 18, 2019, QwazyWabbit add missing texture flagging.
// Process wild-card filenames.
//

#ifdef _WIN32
#if _MSC_VER > 1500
#pragma warning(disable : 4996)
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>	/* for the _find* functions */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define	LUMP_ENTITIES		0
#define	LUMP_PLANES			1
#define	LUMP_VERTEXES		2
#define	LUMP_VISIBILITY		3
#define	LUMP_NODES			4
#define	LUMP_TEXINFO		5
#define	LUMP_FACES			6
#define	LUMP_LIGHTING		7
#define	LUMP_LEAFS			8
#define	LUMP_LEAFFACES		9
#define	LUMP_LEAFBRUSHES	10
#define	LUMP_EDGES			11
#define	LUMP_SURFEDGES		12
#define	LUMP_MODELS			13
#define	LUMP_BRUSHES		14
#define	LUMP_BRUSHSIDES		15
#define	LUMP_POP			16
#define	LUMP_AREAS			17
#define	LUMP_AREAPORTALS	18
#define	HEADER_LUMPS		19

#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
// little-endian "IBSP"

#define BSPVERSION	38

// From qfiles.h
// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define	MAX_MAP_MODELS		1024
#define	MAX_MAP_BRUSHES		8192
#define	MAX_MAP_ENTITIES	2048
#define	MAX_MAP_ENTSTRING	0x40000
#define	MAX_MAP_TEXINFO		8192

#define	MAX_MAP_AREAS		256
#define	MAX_MAP_AREAPORTALS	1024
#define	MAX_MAP_PLANES		65536
#define	MAX_MAP_NODES		65536
#define	MAX_MAP_BRUSHSIDES	65536
#define	MAX_MAP_LEAFS		65536
#define	MAX_MAP_VERTS		65536
#define	MAX_MAP_FACES		65536
#define	MAX_MAP_LEAFFACES	65536
#define	MAX_MAP_LEAFBRUSHES 65536
#define	MAX_MAP_PORTALS		65536
#define	MAX_MAP_EDGES		128000
#define	MAX_MAP_SURFEDGES	256000
#define	MAX_MAP_LIGHTING	0x200000
#define	MAX_MAP_VISIBILITY	0x100000

typedef struct lump_s
{
	int	fileofs;	// file offset of the lump
	int	length;		// length of the lump
} lump_t;

typedef struct dheader_s
{
	int			ident;
	int			version;
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			flags;			// miptex flags + overrides
	int			value;			// light emission, etc
	char		texture[32];	// texture name (textures/*.wal)
	int			nexttexinfo;	// for animations, -1 = end of chain
} texinfo_t;

typedef struct csurface_s
{
	char		name[16];
	int			flags;
	int			value;
} csurface_t;

typedef struct mapsurface_s  // used internally due to name len probs //ZOID
{
	csurface_t	c;
	char		rname[32];
	int			dupe;	//QwazyWabbit// added
} mapsurface_t;

int	LittleLong(int l) { return (l); }

// globals
uint8_t* cmod_base;
int		numtexinfo;
char	map_entitystring[MAX_MAP_ENTSTRING];
mapsurface_t	map_surfaces[MAX_MAP_TEXINFO];
size_t	count_total_missing;

FILE* infile;
char inpath[MAX_PATH];
char drive[_MAX_DRIVE];
char dir[_MAX_DIR];
char fname[_MAX_FNAME];
char ext[_MAX_EXT];
long hFile;
struct _finddata_t file;
int filesize;

#define ERR_CONTINUE 0
#define ERR_DROP 1

int wal_exists(char* name);
void Com_Error(int code, char* fmt, ...);
void CMod_LoadEntityString(lump_t* lump);
void CMod_LoadSurfaces(lump_t* lump);
int FilterFile(FILE* in);
int has_wild(char* fname);
int DrivePath(char* filepath);

int main(int argc, char* argv[])
{
	uint8_t* buf;
	int i;
	int len;
	dheader_t header;
	FILE* in = NULL;

	if (argc > 1)
	{
		if (has_wild(argv[1]))
		{
			DrivePath(argv[1]);
			printf("%lu total missing textures.\n", count_total_missing);
			return EXIT_SUCCESS;
		}
		else
		{
			in = fopen(argv[1], "rb");
			if (!in)
			{
				fprintf(stderr, "FATAL ERROR: fopen() on %s failed.\n", argv[1]);
				return EXIT_FAILURE;
			}
		}
	}
	else
	{
		//print usage info
		printf("Entdump v1.1 is used for extracting entities from quake2 bsp files in text\n");
		printf("format for usage with the added ent file support in Xatrix+ and other mods.\n");
		printf("Wildcard names cause Entdump to output only the texture inventories.\n");
		printf("Usage: entdump mapname.bsp \n");
		printf("   or: entdump mapname.bsp > mapname.txt \n");
		printf("   or: entdump mapname.bsp | more \n");
		return EXIT_FAILURE;
	}

	count_total_missing = 0;
	printf("Opening file: %s\n", argv[1]);
	fseek(in, 0, SEEK_END);
	len = ftell(in);
	fseek(in, 0, SEEK_SET);

	buf = malloc(len);
	filesize = len;

	if (buf)
		fread(buf, len, 1, in);
	else
	{
		printf("Memory allocation failed.\n");
		return EXIT_FAILURE;
	}

	//map header structs onto the buffer
	header = *(dheader_t*)buf;
	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int*)& header)[i] = LittleLong(((int*)& header)[i]);

	if (header.version != BSPVERSION)
		Com_Error(ERR_DROP, "This is not a valid BSP file.");

	//r1: check header pointers point within allocated data
	for (i = 0; i < HEADER_LUMPS; i++)
	{
		//for some reason there are unused lumps with invalid values
		if (i == LUMP_POP)
			continue;

		if (header.lumps[i].fileofs < 0 || header.lumps[i].length < 0 ||
			header.lumps[i].fileofs + header.lumps[i].length > len)
			Com_Error(ERR_DROP, "%s: lump %d offset %d of size %d is out of bounds\n"
				"%s is probably truncated or otherwise corrupted",
				__func__, i, header.lumps[i].fileofs,
				header.lumps[i].length, argv[1]);
	}

	cmod_base = buf;

	printf("Map textures:\n");
	CMod_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);

	printf("Map entities:\n");
	CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES]);

	if (buf)
		free(buf);
	fclose(in);

	return EXIT_SUCCESS;
}

void Com_Error(int code, char* fmt, ...)
{
	va_list argptr;
	static char msg[1024];

	va_start(argptr, fmt);
	vsprintf(msg, fmt, argptr);
	va_end(argptr);

	fprintf(stdout, "ERROR: %s\n", msg);
	if (code == ERR_DROP)
		exit(EXIT_FAILURE);
}

void CMod_LoadEntityString(lump_t* lump)
{

	if (lump->length > MAX_MAP_ENTSTRING)
	{
		Com_Error(ERR_CONTINUE, "Map has too large entity lump (%d > %d)",
			lump->length, MAX_MAP_ENTSTRING);
		return;
	}
	if (lump->fileofs + lump->length > filesize)
	{
		Com_Error(ERR_CONTINUE, "Entity lump parameter error in file %s\n"
			"lump offset %d + length %d exceeds filesize %d\n"
			"the file is truncated or otherwise corrupted.\n",
			file.name, lump->fileofs, lump->length, filesize);
		return;
	}

	memset(map_entitystring, 0, sizeof map_entitystring);
	memcpy(map_entitystring, cmod_base + lump->fileofs, lump->length);

	// remove newline at end of lump string if present.
	if (!strcmp(&map_entitystring[strlen(map_entitystring) - 1], "\n"))
		map_entitystring[strlen(map_entitystring) - 1] = '\0';

	printf("%s\n", map_entitystring);
}

/*
=================
CMod_LoadSurfaces
=================
//QW// pulled this from quake2 engine source and modified it
to list textures used and to flag the missing ones.
*/
void CMod_LoadSurfaces(lump_t* lump)
{
	texinfo_t* in;
	mapsurface_t* out;
	mapsurface_t* list;
	int i;
	int j;
	int count;
	int uniques;
	size_t	count_map_missing;

	count_map_missing = 0;
	in = (void*)(cmod_base + lump->fileofs);
	if (lump->length % sizeof(*in))
	{
		Com_Error(ERR_CONTINUE, "%s: funny lump size in %s",
			__func__, file.name);
		return;
	}
	count = lump->length / sizeof(*in);
	if (count < 1)
	{
		Com_Error(ERR_CONTINUE, "%s: Map with no surfaces: %s",
			__func__, file.name);
		return;
	}
	if (count > MAX_MAP_TEXINFO)
	{
		Com_Error(ERR_CONTINUE, "%s: Map has too many surfaces: %s",
			__func__, file.name);
		return;
	}

	numtexinfo = count;
	out = map_surfaces;
	uniques = 0;

	for (i = 0; i < count; i++, in++, out++)
	{
		strncpy(out->c.name, in->texture, sizeof(out->c.name) - 1);
		strncpy(out->rname, in->texture, sizeof(out->rname) - 1);
		out->c.flags = LittleLong(in->flags);
		out->c.value = LittleLong(in->value);
		out->dupe = 0;

		list = map_surfaces;
		for (j = 0; j < count; j++, list++)	// identify each unique texture name
		{
			if (strcmp(list->rname, "") != 0
				&& list != out
				&& strcmp(list->rname, out->rname) == 0)
				out->dupe = 1;	//flag the duplicate
		}
		if (!out->dupe)
		{
			uniques++;
			if (wal_exists(out->rname))
				printf("textures/%s.wal\n", out->rname);
			else {
				printf("textures/%s.wal file is MISSING for %s\n", out->rname, file.name);
				count_map_missing++;
				count_total_missing++;
			}
		}
	}
	printf("Map uses %i unique textures %i times\n", uniques, count);
	printf("Missing %lu textures.\n", count_map_missing);
}

int wal_exists(char* name)
{
	char wal_name[_MAX_PATH];
	FILE* f;

	sprintf(wal_name, "/quake2/baseq2/textures/%s.wal", name);
	f = fopen(wal_name, "r");
	if (f) {
		fclose(f);
		return 1;
	}
	return 0;
}

int has_wild(char* filename)
{
	char c;

	while ((c = *filename++) != 0)
		if (c == '*' || c == '?')
			return 1;
	return 0;
}

/*
 Iterate over the wild-carded files
 listing the textures used in each.
*/
int DrivePath(char* filepath)
{
	unsigned status = 0;
	int nfiles = 0;
	int num;

	if ((hFile = _findfirst(filepath, &file)) == -1L)
	{
		printf("No files named %s found.\n", filepath);
		status = 1;	//error status
	}
	else
	{
		nfiles++;
		while (_findnext(hFile, &file) == 0)
		{
			nfiles++;
		}
		_findclose(hFile);
		hFile = _findfirst(filepath, &file);
		// the file is "found" without path spec
		// so we have to rebuild the path
		_splitpath(filepath, drive, dir, fname, ext);
		_makepath(inpath, drive, dir, file.name, NULL);
		printf("Opening %s\n", file.name);
		infile = fopen(inpath, "r");
		if (infile)
		{
			status = FilterFile(infile);
			fclose(infile);
		}
		for (num = 1; num < nfiles; ++num)
		{
			if (_findnext(hFile, &file) != 0)
				Com_Error(ERR_DROP, "Whoops, couldn't find next file!");
			_splitpath(filepath, drive, dir, fname, ext);
			_makepath(inpath, drive, dir, file.name, NULL);
			printf("Opening %s\n", file.name);
			infile = fopen(inpath, "r");
			if (infile)
			{
				status = FilterFile(infile);
				fclose(infile);
			}
		}
	}
	printf("%i map files processed.\n", nfiles);
	_findclose(hFile);
	return (status);
}

// List the textures used in the map
// identify those that are missing.
// NOTE: this function outputs only the .wal files
// and it gets called if we're wild-carding
// the map file names in the command line.
int FilterFile(FILE* in)
{
	uint8_t* buf = NULL;
	int		i;
	int		len;
	dheader_t	header;

	fseek(in, 0, SEEK_END);
	len = ftell(in);
	fseek(in, 0, SEEK_SET);

	buf = malloc(len);
	filesize = len;

	if (buf)
		fread(buf, len, 1, in);
	else
	{
		printf("Memory allocation failed.\n");
		return EXIT_FAILURE;
	}

	//map header structs onto the buffer
	header = *(dheader_t*)buf;
	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int*)& header)[i] = LittleLong(((int*)& header)[i]);

	//r1: check header pointers point within allocated data
	for (i = 0; i < HEADER_LUMPS; i++)
	{
		//for some reason there are unused lumps with invalid values
		if (i == LUMP_POP)
			continue;

		if (header.lumps[i].fileofs < 0 || header.lumps[i].length < 0 ||
			header.lumps[i].fileofs + header.lumps[i].length > len)
		{
			Com_Error(ERR_DROP,
				"%s: lump %d offset %d of size %d is out of bounds\n"
				"%s is probably truncated or otherwise corrupted",
				__func__, i, header.lumps[i].fileofs,
				header.lumps[i].length, file.name);
		}
	}

	if (header.version != BSPVERSION)
	{
		Com_Error(ERR_CONTINUE, "%s is not a valid BSP file.", file.name);
	}
	else
	{
		cmod_base = buf;
		printf("Map textures:\n");
		CMod_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);
	}

	if (buf)
		free(buf);
	return 0;
}
