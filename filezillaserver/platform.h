// üPlatform.h
//
//////////////////////////////////////////////////////////////////////

#if !defined(_PLATFORM_INCLUDED_)
#define _PLATFORM_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef _X86_
// X86 allows unaligned accesses, so we can just dereference any pointer
// after a cast
#define GET16(pc)	(*(WORD *)(pc))
#define GET32(pc)	(*(DWORD *)(pc))
#define GET64(pc)	(*(DWORD64 *)(pc))
#else // not _X86_
// Other platforms (IPF and AMD64) either have a have a significant penalty
// for unaligned accesses or they may trap to the OS).
// For these platformsm we put the bytes together to make 16, 32 and 64 bit
// objects.  We assume little-endian data.
#define GET16(pc)	((WORD)(*(unsigned char *)(pc)) |	\
		((*(unsigned char *)((pc) + 1)) << 8))
#define GET32(pc)	((DWORD)(*(unsigned char *)(pc)) |	\
		((*(unsigned char *)((pc) + 1)) << 8) |		\
		((*(unsigned char *)((pc) + 2)) << 16) |	\
		((*(unsigned char *)((pc) + 3)) << 24))
#define GET64(pc)	((DWORD64)(*(unsigned char *)(pc)) |	\
		((*(unsigned char *)((pc) + 1)) << 8) |		\
		((*(unsigned char *)((pc) + 2)) << 16) |	\
		((*(unsigned char *)((pc) + 3)) << 24) |	\
		((*(unsigned char *)((pc) + 4)) << 32) |	\
		((*(unsigned char *)((pc) + 5)) << 40) |	\
		((*(unsigned char *)((pc) + 6)) << 48) |	\
		((*(unsigned char *)((pc) + 7)) << 56))
#endif // if _X86_ ... else not _X86_ ...

#endif // !defined(_PLATFORM_INCLUDED_)
