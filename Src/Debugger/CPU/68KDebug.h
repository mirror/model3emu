#ifdef SUPERMODEL_DEBUGGER
#ifdef SUPERMODEL_SOUND
#ifndef INCLUDED_68KDEBUG_H
#define INCLUDED_68KDEBUG_H

#include "Debugger/CPUDebug.h"
#include "Types.h"

#include "CPU/68K/Turbo68K.h"

#define M68KSPECIAL_SP 0
#define M68KSPECIAL_SR 1

#define USE_NATIVE_READ 0
#define USE_NATIVE_WRITE 0
	
namespace Debugger
{
	class C68KDebug;

	static C68KDebug *debug = NULL;

	typedef bool (*DebugPtr)(TURBO68K_INT32 pc, TURBO68K_INT32 opcode);
	typedef void (*IntAckPtr)(TURBO68K_UINT32 intVec);

	static DebugPtr origDebugPtr = NULL;
	static IntAckPtr origIntAckPtr = NULL;

	static TURBO68K_DATAREGION *origRead8Regions;
	static TURBO68K_DATAREGION *origRead16Regions;
	static TURBO68K_DATAREGION *origRead32Regions;
	static TURBO68K_DATAREGION *origWrite8Regions;
	static TURBO68K_DATAREGION *origWrite16Regions;
	static TURBO68K_DATAREGION *origWrite32Regions;

	static TURBO68K_DATAREGION debugRead8Regions[] =
	{
		{ 0x000000, 0xFFFFFF, TURBO68K_NULL, TURBO68K_NULL },
		{ -1,		-1,		  TURBO68K_NULL, TURBO68K_NULL }
	};
	static TURBO68K_DATAREGION debugRead16Regions[] =
	{
		{ 0x000000, 0xFFFFFF, TURBO68K_NULL, TURBO68K_NULL },
		{ -1,		-1,		  TURBO68K_NULL, TURBO68K_NULL }
	};
	static TURBO68K_DATAREGION debugRead32Regions[] =
	{
		{ 0x000000, 0xFFFFFF, TURBO68K_NULL, TURBO68K_NULL },
		{ -1,		-1,		  TURBO68K_NULL, TURBO68K_NULL }
	};
	static TURBO68K_DATAREGION debugWrite8Regions[] =
	{
		{ 0x000000, 0xFFFFFF, TURBO68K_NULL, TURBO68K_NULL },
		{ -1,		-1,		  TURBO68K_NULL, TURBO68K_NULL }
	};
	static TURBO68K_DATAREGION debugWrite16Regions[] =
	{
		{ 0x000000, 0xFFFFFF, TURBO68K_NULL, TURBO68K_NULL },
		{ -1,		-1,		  TURBO68K_NULL, TURBO68K_NULL }
	};
	static TURBO68K_DATAREGION debugWrite32Regions[] =
	{
		{ 0x000000, 0xFFFFFF, TURBO68K_NULL, TURBO68K_NULL },
		{ -1,		-1,		  TURBO68K_NULL, TURBO68K_NULL }
	};

	static UINT32 GetSpecialReg(CCPUDebug *cpu, unsigned id);
	static bool SetSpecialReg(CCPUDebug *cpu, unsigned id, UINT32 data);
	static UINT32 GetDataReg(CCPUDebug *cpu, unsigned id);
	static bool SetDataReg(CCPUDebug *cpu, unsigned id, UINT32 data) ;
	static UINT32 GetAddressReg(CCPUDebug *cpu, unsigned id);
	static bool SetAddressReg(CCPUDebug *cpu, unsigned id, UINT32 data);

	static TURBO68K_UINT8 ReadByteDebug(TURBO68K_UINT32 addr);
	static TURBO68K_UINT16 ReadWordDebug(TURBO68K_UINT32 addr);
	static TURBO68K_UINT32 ReadLongDebug(TURBO68K_UINT32 addr);
	static void WriteByteDebug(TURBO68K_UINT32 addr, TURBO68K_UINT8 data);
	static void WriteWordDebug(TURBO68K_UINT32 addr, TURBO68K_UINT16 data);
	static void WriteLongDebug(TURBO68K_UINT32 addr, TURBO68K_UINT32 data);

	static TURBO68K_UINT8 ReadByteDirect(TURBO68K_UINT32 addr);
	static TURBO68K_UINT16 ReadWordDirect(TURBO68K_UINT32 addr);
	static TURBO68K_UINT32 ReadLongDirect(TURBO68K_UINT32 addr);
	static void WriteByteDirect(TURBO68K_UINT32 addr, TURBO68K_UINT8 data);
	static void WriteWordDirect(TURBO68K_UINT32 addr, TURBO68K_UINT16 data);
	static void WriteLongDirect(TURBO68K_UINT32 addr, TURBO68K_UINT32 data);

	/*
	 * CCPUDebug implementation for the Turbo68K Motorola 68000 emulator.
	 */
	class C68KDebug : public CCPUDebug
	{
	private:
		char m_drNames[8][3];
		char m_arNames[8][3];

		char m_mSlotStr[32][20];
		char m_sSlotStr[32][20];
		char m_regStr[16][12];

		UINT32 m_resetAddr;

		bool FormatAddrMode(UINT32 addr, UINT32 opcode, int &offset, UINT8 addrMode, char sizeC, char *dest);

	public:
		C68KDebug();

		virtual ~C68KDebug();

		// CCPUDebug methods

		void AttachToCPU();

		void DetachFromCPU();

		UINT32 GetResetAddr();

		bool UpdatePC(UINT32 pc);

		bool ForceException(CException *ex);

		bool ForceInterrupt(CInterrupt *in);

		UINT64 ReadMem(UINT32 addr, unsigned dataSize);

		bool WriteMem(UINT32 addr, unsigned dataSize, UINT64 data);

		int Disassemble(UINT32 addr, char *mnemonic, char *operands);
		
		EOpFlags GetOpFlags(UINT32 addr, UINT32 opcode);

		bool GetJumpAddr(UINT32 addr, UINT32 opcode, UINT32 &jumpAddr);

		bool GetJumpRetAddr(UINT32 addr, UINT32 opcode, UINT32 &retAddr);

		bool GetReturnAddr(UINT32 addr, UINT32 opcode, UINT32 &retAddr);

		bool GetHandlerAddr(CException *ex, UINT32 &handlerAddr);

		bool GetHandlerAddr(CInterrupt *in, UINT32 &handlerAddr);
	};

	//
	// Inlined functions
	//

	typedef TURBO68K_UINT8 (*ReadByteFPtr)(TURBO68K_UINT32);
	typedef TURBO68K_UINT16 (*ReadWordFPtr)(TURBO68K_UINT32);
	typedef TURBO68K_UINT32 (*ReadLongFPtr)(TURBO68K_UINT32);
	typedef void (*WriteByteFPtr)(TURBO68K_UINT32, TURBO68K_UINT8);
	typedef void (*WriteWordFPtr)(TURBO68K_UINT32, TURBO68K_UINT16);
	typedef void (*WriteLongFPtr)(TURBO68K_UINT32, TURBO68K_UINT32);

	inline TURBO68K_UINT8 ReadByteDirect(TURBO68K_UINT32 addr)
	{
#if USE_NATIVE_READ
		Turbo68KSetReadByte(origRead8Regions, TURBO68K_NULL);
		TURBO68K_UINT8 data = Turbo68KReadByte(addr);
		Turbo68KSetReadByte(debugRead8Regions, TURBO68K_NULL);
		return data;
#else
		for (TURBO68K_DATAREGION *region = origRead8Regions; region->ptr != TURBO68K_NULL || region->handler != TURBO68K_NULL; region++)
		{
			if (region->base <= addr && addr <= region->limit)
			{
				if (region->ptr != TURBO68K_NULL)
				{
					// Turbo68K requires native memory to be byte swapped, so must reverse this when reading bytes
					TURBO68K_UINT8 *dataP = (TURBO68K_UINT8*)(region->ptr + (addr^1));
					return *dataP;
				}
				else
				{
					ReadByteFPtr fPtr = (ReadByteFPtr)region->handler;
					return fPtr(addr);
				}
			}
		}
		return 0;
#endif
	}

	inline TURBO68K_UINT16 ReadWordDirect(TURBO68K_UINT32 addr)
	{
#if USE_NATIVE_READ
		Turbo68KSetReadWord(origRead16Regions, TURBO68K_NULL);
		TURBO68K_UINT16 data = Turbo68KReadWord(addr);
		Turbo68KSetReadWord(debugRead16Regions, TURBO68K_NULL);
		return data;
#else
		for (TURBO68K_DATAREGION *region = origRead16Regions; region->ptr != TURBO68K_NULL || region->handler != TURBO68K_NULL; region++)
		{
			if (region->base <= addr && addr <= region->limit)
			{
				if (region->ptr != TURBO68K_NULL)
				{
					// Turbo68K requires native memory to be byte swapped, so must reverse this when reading bytes
					TURBO68K_UINT16 *dataP = (TURBO68K_UINT16*)(region->ptr + addr);
					return *dataP;
				}
				else
				{
					ReadWordFPtr fPtr = (ReadWordFPtr)region->handler;
					return fPtr(addr);
				}
			}
		}
		return 0;
#endif
	}

	inline TURBO68K_UINT32 ReadLongDirect(TURBO68K_UINT32 addr)
	{
#if USE_NATIVE_READ
		Turbo68KSetReadLong(origRead32Regions, TURBO68K_NULL);
		TURBO68K_UINT32 data = Turbo68KReadLong(addr);
		Turbo68KSetReadLong(debugRead32Regions, TURBO68K_NULL);
		return data;
#else
		for (TURBO68K_DATAREGION *region = origRead32Regions; region->ptr != TURBO68K_NULL || region->handler != TURBO68K_NULL; region++)
		{
			if (region->base <= addr && addr <= region->limit)
			{
				if (region->ptr != TURBO68K_NULL)
				{
					// Turbo68K requires native memory to be byte swapped, so must reverse this when reading bytes
					TURBO68K_UINT16 *dataP = (TURBO68K_UINT16*)(region->ptr + addr);
					return (TURBO68K_UINT32)dataP[1] | ((TURBO68K_UINT32)dataP[0])<<16;
				}
				else
				{
					ReadLongFPtr fPtr = (ReadLongFPtr)region->handler;
					return fPtr(addr);
				}
			}
		}
		return 0;
#endif
	}

	inline void WriteByteDirect(TURBO68K_UINT32 addr, TURBO68K_UINT8 data)
	{
#if USE_NATIVE_WRITE
		Turbo68KSetWriteByte(origWrite8Regions, TURBO68K_NULL);
		Turbo68KWriteByte(addr, data);
		Turbo68KSetWriteByte(debugWrite8Regions, TURBO68K_NULL);
#else
		for (TURBO68K_DATAREGION *region = origWrite8Regions; region->ptr != TURBO68K_NULL || region->handler != TURBO68K_NULL; region++)
		{
			if (region->base <= addr && addr <= region->limit)
			{
				if (region->ptr != TURBO68K_NULL)
				{
					// Turbo68K requires native memory to be byte swapped, so must reverse this when writing bytes
					TURBO68K_UINT8 *dataP = (TURBO68K_UINT8*)(region->ptr + (addr^1));
					*dataP = data;
					return;
				}
				else
				{
					WriteByteFPtr fPtr = (WriteByteFPtr)region->handler;
					fPtr(addr, data);
					return;
				}
			}
		}
		return;
#endif
	}

	inline void WriteWordDirect(TURBO68K_UINT32 addr, TURBO68K_UINT16 data)
	{
#if USE_NATIVE_WRITE
		Turbo68KSetWriteWord(origWrite16Regions, TURBO68K_NULL);
		Turbo68KWriteWord(addr, data);
		Turbo68KSetWriteWord(debugWrite16Regions, TURBO68K_NULL);
#else
		for (TURBO68K_DATAREGION *region = origWrite16Regions; region->ptr != TURBO68K_NULL || region->handler != TURBO68K_NULL; region++)
		{
			if (region->base <= addr && addr <= region->limit)
			{
				if (region->ptr != TURBO68K_NULL)
				{
					// Turbo68K requires native memory to be byte swapped, so must reverse this when writing bytes
					TURBO68K_UINT16 *dataP = (TURBO68K_UINT16*)(region->ptr + addr);
					*dataP = data;
					return;
				}
				else
				{
					WriteWordFPtr fPtr = (WriteWordFPtr)region->handler;
					fPtr(addr, data);
					return;
				}
			}
		}
		return;
#endif
	}

	inline void WriteLongDirect(TURBO68K_UINT32 addr, TURBO68K_UINT32 data)
	{
#if USE_NATIVE_WRITE
		Turbo68KSetWriteLong(origWrite32Regions, TURBO68K_NULL);
		Turbo68KWriteLong(addr, data);
		Turbo68KSetWriteLong(debugWrite32Regions, TURBO68K_NULL);
#else
		for (TURBO68K_DATAREGION *region = origWrite32Regions; region->ptr != TURBO68K_NULL || region->handler != TURBO68K_NULL; region++)
		{
			if (region->base <= addr && addr <= region->limit)
			{
				if (region->ptr != TURBO68K_NULL)
				{
					// Turbo68K requires native memory to be byte swapped, so must reverse this when writing bytes
					TURBO68K_UINT16 *dataP = (TURBO68K_UINT16*)(region->ptr + addr);
					dataP[0] = data>>16;
					dataP[1] = data&0xFFFF;
					return;
				}
				else
				{
					WriteLongFPtr fPtr = (WriteLongFPtr)region->handler;
					fPtr(addr, data);
					return;
				}
			}
		}
		return;
#endif
	}
}

#endif	// INCLUDED_68KDEBUG_H
#endif  // SUPERMODEL_SOUND
#endif  // SUPERMODEL_DEBUGGER