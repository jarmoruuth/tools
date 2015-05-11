/**********************************************************************\
 *
 *	ADAPTER.C
 *
 * Adapter type checking routine from Dr. Dobb's Journal, Feb. 1989
 *
\**********************************************************************/

typedef enum {
	NONE,
	MDA,
	CGA,
	EGACOLOR,
	EGAMONO,
	VGAMONO,
	VGACOLOR,
	MCGACOLOR,
	MCGAMONO
} adapter_type_t;

adapter_type_t query_adapter_type(void)
{
	rg.h.ah = 0x1a;
	rg.h.al = 0x00;
	int86(0x10, &rg, &rg);
	if (rg.h.al == 0x1a) {		/* we have PS/2 BIOS */
		switch (rg.h.bl) {
			case 0x00:
				return NONE;
			case 0x01:
				return MDA;
			case 0x02:
				return CGA;
			case 0x04:
				return EGACOLOR;
			case 0x05:
				return EGAMONO;
			case 0x07:
				return VGAMONO;
			case 0x08:
				return VGACOLOR;
			case 0x0a:
			case 0x0c:
				return MCGACOLOR;
			case 0x0b:
				return MCGAMONO;
			default:
				return CGA;
		}
	} else {			/* non-PS/2 BIOS */
		rg.h.ah = 0x12;
		rg.x.bx = 0x10;
		int86(0x10, &rg, &rg);
		if (rg.x.bx != 0x10) {	/* EGA */
			rg.h.ah = 0x12;
			rg.h.bl = 0x10;
			int86(0x10, &rg, &rg);
			if (rg.h.bh == 0)
				return EGACOLOR;
			else
				return EGAMONO;
		} else {		/* CGA or MDA */
			int86(0x11, &rg, &rg);
			switch ((rg.h.al & 0x30) >> 4) {
				case 1:
				case 2:
					return CGA;
				case 3:
					return MDA;
				default:
					return NONE;
			}
		}
	}
}
