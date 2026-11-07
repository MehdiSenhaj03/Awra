/* Indic syllabic and Indic_Conjunct_Break properties for grapheme
 * boundaries (UAX #29 GB9c) and backspace, taken from Pango's generated
 * pango-break-table.h (Unicode 17.0.0 data; Pango is LGPL-2.1-or-later,
 * the data is under the Unicode license). Only the types were renamed for
 * BUI: gunichar -> uint32_t, gboolean -> int.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */

#ifndef BUI_BREAK_PROPS_H
#define BUI_BREAK_PROPS_H

static inline int
bui_is_Virama (uint32_t wc)
{
  if ((wc >= 0x094D && wc <= 0x0BCD))
    {
      if (wc == 0x094D)
	return 1;
      if (wc == 0x09CD || wc == 0x0A4D)
	return 1;
      if (wc == 0x0ACD)
	return 1;
      if (wc == 0x0B4D || wc == 0x0BCD)
	return 1;
      return 0;
    }
  if ((wc >= 0x0C4D && wc <= 0xA8C4))
    {
      if (wc == 0x0C4D)
	return 1;
      if (wc == 0x0CCD || wc == 0x0D4D)
	return 1;
      if (wc == 0x0DCA)
	return 1;
      if (wc == 0x1B44 || wc == 0xA806 || wc == 0xA8C4)
	return 1;
      return 0;
    }
  if ((wc >= 0xA9C0 && wc <= 0x1134D))
    {
      if (wc == 0xA9C0)
	return 1;
      if (wc == 0x11046 || wc == 0x110B9)
	return 1;
      if (wc == 0x111C0)
	return 1;
      if (wc == 0x11235 || wc == 0x1134D)
	return 1;
      return 0;
    }
  if ((wc >= 0x11442 && wc <= 0x11C3F))
    {
      if (wc == 0x11442 || wc == 0x114C2)
	return 1;
      if (wc == 0x115BF || wc == 0x1163F)
	return 1;
      if (wc == 0x116B6 || wc == 0x11839)
	return 1;
      if (wc == 0x119E0 || wc == 0x11C3F)
	return 1;
      return 0;
    }
  return 0;
}

static inline int
bui_is_Vowel_Dependent (uint32_t wc)
{
  if ((wc >= 0x093A && wc <= 0x0CE3))
    {
      if ((wc >= 0x093A && wc <= 0x09D7))
	{
	  if ((wc >= 0x093A && wc <= 0x093B) ||
	      (wc >= 0x093E && wc <= 0x094C))
	    return 1;
	  if ((wc >= 0x094E && wc <= 0x094F) ||
	      (wc >= 0x0955 && wc <= 0x0957))
	    return 1;
	  if ((wc >= 0x0962 && wc <= 0x0963) ||
	      (wc >= 0x09BE && wc <= 0x09C4))
	    return 1;
	  if ((wc >= 0x09C7 && wc <= 0x09C8) ||
	      (wc >= 0x09CB && wc <= 0x09CC) || wc == 0x09D7)
	    return 1;
	  return 0;
	}
      if ((wc >= 0x09E2 && wc <= 0x0B44))
	{
	  if ((wc >= 0x09E2 && wc <= 0x09E3) ||
	      (wc >= 0x0A3E && wc <= 0x0A42))
	    return 1;
	  if ((wc >= 0x0A47 && wc <= 0x0A48) ||
	      (wc >= 0x0A4B && wc <= 0x0A4C))
	    return 1;
	  if ((wc >= 0x0ABE && wc <= 0x0AC5) ||
	      (wc >= 0x0AC7 && wc <= 0x0AC9))
	    return 1;
	  if ((wc >= 0x0ACB && wc <= 0x0ACC) ||
	      (wc >= 0x0AE2 && wc <= 0x0AE3) ||
	      (wc >= 0x0B3E && wc <= 0x0B44))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x0B47 && wc <= 0x0C44))
	{
	  if ((wc >= 0x0B47 && wc <= 0x0B48) ||
	      (wc >= 0x0B4B && wc <= 0x0B4C))
	    return 1;
	  if ((wc >= 0x0B55 && wc <= 0x0B57) ||
	      (wc >= 0x0B62 && wc <= 0x0B63))
	    return 1;
	  if ((wc >= 0x0BBE && wc <= 0x0BC2) ||
	      (wc >= 0x0BC6 && wc <= 0x0BC8))
	    return 1;
	  if ((wc >= 0x0BCA && wc <= 0x0BCC) ||
	      wc == 0x0BD7 || (wc >= 0x0C3E && wc <= 0x0C44))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x0C46 && wc <= 0x0CE3))
	{
	  if ((wc >= 0x0C46 && wc <= 0x0C48) ||
	      (wc >= 0x0C4A && wc <= 0x0C4C))
	    return 1;
	  if ((wc >= 0x0C55 && wc <= 0x0C56) ||
	      (wc >= 0x0C62 && wc <= 0x0C63))
	    return 1;
	  if ((wc >= 0x0CBE && wc <= 0x0CC4) ||
	      (wc >= 0x0CC6 && wc <= 0x0CC8))
	    return 1;
	  if ((wc >= 0x0CCA && wc <= 0x0CCC) ||
	      (wc >= 0x0CD5 && wc <= 0x0CD6) ||
	      (wc >= 0x0CE2 && wc <= 0x0CE3))
	    return 1;
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x0D3E && wc <= 0x1B43))
    {
      if ((wc >= 0x0D3E && wc <= 0x0DF3))
	{
	  if ((wc >= 0x0D3E && wc <= 0x0D44) ||
	      (wc >= 0x0D46 && wc <= 0x0D48))
	    return 1;
	  if ((wc >= 0x0D4A && wc <= 0x0D4C) || wc == 0x0D57)
	    return 1;
	  if ((wc >= 0x0D62 && wc <= 0x0D63) ||
	      (wc >= 0x0DCF && wc <= 0x0DD4))
	    return 1;
	  if (wc == 0x0DD6 ||
	      (wc >= 0x0DD8 && wc <= 0x0DDF) ||
	      (wc >= 0x0DF2 && wc <= 0x0DF3))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x0E30 && wc <= 0x1035))
	{
	  if ((wc >= 0x0E30 && wc <= 0x0E39) ||
	      (wc >= 0x0E40 && wc <= 0x0E45))
	    return 1;
	  if (wc == 0x0E47 || (wc >= 0x0EB0 && wc <= 0x0EB9))
	    return 1;
	  if (wc == 0x0EBB || (wc >= 0x0EC0 && wc <= 0x0EC4))
	    return 1;
	  if ((wc >= 0x0F71 && wc <= 0x0F7D) ||
	      (wc >= 0x0F80 && wc <= 0x0F81) ||
	      (wc >= 0x102B && wc <= 0x1035))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x1056 && wc <= 0x1753))
	{
	  if ((wc >= 0x1056 && wc <= 0x1059) || wc == 0x1062)
	    return 1;
	  if ((wc >= 0x1067 && wc <= 0x1068) ||
	      (wc >= 0x1071 && wc <= 0x1074))
	    return 1;
	  if ((wc >= 0x1083 && wc <= 0x1086) ||
	      (wc >= 0x109C && wc <= 0x109D))
	    return 1;
	  if ((wc >= 0x1712 && wc <= 0x1713) ||
	      (wc >= 0x1732 && wc <= 0x1733) ||
	      (wc >= 0x1752 && wc <= 0x1753))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x1772 && wc <= 0x1B43))
	{
	  if ((wc >= 0x1772 && wc <= 0x1773) ||
	      (wc >= 0x17B6 && wc <= 0x17C5))
	    return 1;
	  if (wc == 0x17C8 || (wc >= 0x1920 && wc <= 0x1928))
	    return 1;
	  if (wc == 0x193A || (wc >= 0x19B0 && wc <= 0x19C0))
	    return 1;
	  if ((wc >= 0x1A17 && wc <= 0x1A1B) ||
	      (wc >= 0x1A61 && wc <= 0x1A73) ||
	      (wc >= 0x1B35 && wc <= 0x1B43))
	    return 1;
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x1BA4 && wc <= 0x113C2))
    {
      if ((wc >= 0x1BA4 && wc <= 0xA9BC))
	{
	  if ((wc >= 0x1BA4 && wc <= 0x1BA9) ||
	      (wc >= 0x1BE7 && wc <= 0x1BEF))
	    return 1;
	  if ((wc >= 0x1C26 && wc <= 0x1C2C) || wc == 0xA802)
	    return 1;
	  if ((wc >= 0xA823 && wc <= 0xA827) ||
	      (wc >= 0xA8B5 && wc <= 0xA8C3))
	    return 1;
	  if (wc == 0xA8FF ||
	      (wc >= 0xA947 && wc <= 0xA94E) ||
	      (wc >= 0xA9B4 && wc <= 0xA9BC))
	    return 1;
	  return 0;
	}
      if ((wc >= 0xA9E5 && wc <= 0x11045))
	{
	  if (wc == 0xA9E5 || (wc >= 0xAA29 && wc <= 0xAA32))
	    return 1;
	  if ((wc >= 0xAAB0 && wc <= 0xAABE) ||
	      (wc >= 0xAAEB && wc <= 0xAAEF))
	    return 1;
	  if ((wc >= 0xABE3 && wc <= 0xABEA) ||
	      (wc >= 0x10A01 && wc <= 0x10A03))
	    return 1;
	  if ((wc >= 0x10A05 && wc <= 0x10A06) ||
	      (wc >= 0x10A0C && wc <= 0x10A0D) ||
	      (wc >= 0x11038 && wc <= 0x11045))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x11073 && wc <= 0x11233))
	{
	  if ((wc >= 0x11073 && wc <= 0x11074) ||
	      (wc >= 0x110B0 && wc <= 0x110B8))
	    return 1;
	  if (wc == 0x110C2 || (wc >= 0x11127 && wc <= 0x11132))
	    return 1;
	  if ((wc >= 0x11145 && wc <= 0x11146) ||
	      (wc >= 0x111B3 && wc <= 0x111BF))
	    return 1;
	  if ((wc >= 0x111CB && wc <= 0x111CC) ||
	      wc == 0x111CE || (wc >= 0x1122C && wc <= 0x11233))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x11241 && wc <= 0x113C2))
	{
	  if (wc == 0x11241 || (wc >= 0x112E0 && wc <= 0x112E8))
	    return 1;
	  if ((wc >= 0x1133E && wc <= 0x11344) ||
	      (wc >= 0x11347 && wc <= 0x11348))
	    return 1;
	  if ((wc >= 0x1134B && wc <= 0x1134C) || wc == 0x11357)
	    return 1;
	  if ((wc >= 0x11362 && wc <= 0x11363) ||
	      (wc >= 0x113B8 && wc <= 0x113C0) || wc == 0x113C2)
	    return 1;
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x113C5 && wc <= 0x16D6A))
    {
      if ((wc >= 0x113C5 && wc <= 0x11640))
	{
	  if (wc == 0x113C5 || (wc >= 0x113C7 && wc <= 0x113C9))
	    return 1;
	  if ((wc >= 0x11435 && wc <= 0x11441) ||
	      (wc >= 0x114B0 && wc <= 0x114BE))
	    return 1;
	  if ((wc >= 0x115AF && wc <= 0x115B5) ||
	      (wc >= 0x115B8 && wc <= 0x115BB))
	    return 1;
	  if ((wc >= 0x115DC && wc <= 0x115DD) ||
	      (wc >= 0x11630 && wc <= 0x1163C) || wc == 0x11640)
	    return 1;
	  return 0;
	}
      if ((wc >= 0x116AD && wc <= 0x11A0A))
	{
	  if ((wc >= 0x116AD && wc <= 0x116B5) ||
	      (wc >= 0x11720 && wc <= 0x1172A))
	    return 1;
	  if ((wc >= 0x1182C && wc <= 0x11836) ||
	      (wc >= 0x11930 && wc <= 0x11935))
	    return 1;
	  if ((wc >= 0x11937 && wc <= 0x11938) ||
	      (wc >= 0x119D1 && wc <= 0x119D7))
	    return 1;
	  if ((wc >= 0x119DA && wc <= 0x119DD) ||
	      wc == 0x119E4 || (wc >= 0x11A01 && wc <= 0x11A0A))
	    return 1;
	  return 0;
	}
      if ((wc >= 0x11A51 && wc <= 0x11D3F))
	{
	  if ((wc >= 0x11A51 && wc <= 0x11A5B) ||
	      (wc >= 0x11B60 && wc <= 0x11B67))
	    return 1;
	  if ((wc >= 0x11C2F && wc <= 0x11C36) ||
	      (wc >= 0x11C38 && wc <= 0x11C3B))
	    return 1;
	  if ((wc >= 0x11CB0 && wc <= 0x11CB4) ||
	      (wc >= 0x11D31 && wc <= 0x11D36))
	    return 1;
	  if (wc == 0x11D3A ||
	      (wc >= 0x11D3C && wc <= 0x11D3D) || wc == 0x11D3F)
	    return 1;
	  return 0;
	}
      if ((wc >= 0x11D43 && wc <= 0x16D6A))
	{
	  if (wc == 0x11D43 || (wc >= 0x11D8A && wc <= 0x11D8E))
	    return 1;
	  if ((wc >= 0x11D90 && wc <= 0x11D91) ||
	      (wc >= 0x11D93 && wc <= 0x11D94))
	    return 1;
	  if ((wc >= 0x11EF3 && wc <= 0x11EF6) ||
	      (wc >= 0x11F34 && wc <= 0x11F3A))
	    return 1;
	  if ((wc >= 0x11F3E && wc <= 0x11F40) ||
	      (wc >= 0x1611E && wc <= 0x16129) ||
	      (wc >= 0x16D63 && wc <= 0x16D6A))
	    return 1;
	  return 0;
	}
      return 0;
    }
  return 0;
}

static inline int
bui_is_Consonant_Prefixed (uint32_t wc)
{
  if ((wc >= 0x111C2 && wc <= 0x111C3) ||
      wc == 0x1193F ||
      (wc >= 0x11A84 && wc <= 0x11A85) || (wc >= 0x11A87 && wc <= 0x11A89))
    return 1;
  return 0;
}

static inline int
bui_is_Consonant_Preceding_Repha (uint32_t wc)
{
  if (wc == 0x0D4E)
    return 1;
  if (wc == 0x113D1 || wc == 0x11941)
    return 1;
  if (wc == 0x11A86)
    return 1;
  if (wc == 0x11D46 || wc == 0x11F02)
    return 1;
  return 0;
}

static inline int
bui_is_Indic_Conjunct_Break_Linker (uint32_t wc)
{
  if ((wc >= 0x094D && wc <= 0x0C4D))
    {
      if (wc == 0x094D)
	return 1;
      if (wc == 0x09CD)
	return 1;
      if (wc == 0x0ACD)
	return 1;
      if (wc == 0x0B4D || wc == 0x0C4D)
	return 1;
      return 0;
    }
  if ((wc >= 0x0D4D && wc <= 0x1B44))
    {
      if (wc == 0x0D4D)
	return 1;
      if (wc == 0x1039)
	return 1;
      if (wc == 0x17D2)
	return 1;
      if (wc == 0x1A60 || wc == 0x1B44)
	return 1;
      return 0;
    }
  if ((wc >= 0x1BAB && wc <= 0x11133))
    {
      if (wc == 0x1BAB)
	return 1;
      if (wc == 0xA9C0)
	return 1;
      if (wc == 0xAAF6)
	return 1;
      if (wc == 0x10A3F || wc == 0x11133)
	return 1;
      return 0;
    }
  if ((wc >= 0x113D0 && wc <= 0x11F42))
    {
      if (wc == 0x113D0)
	return 1;
      if (wc == 0x1193E)
	return 1;
      if (wc == 0x11A47)
	return 1;
      if (wc == 0x11A99 || wc == 0x11F42)
	return 1;
      return 0;
    }
  return 0;
}

static inline int
bui_is_Indic_Conjunct_Break_Consonant (uint32_t wc)
{
  if ((wc >= 0x0915 && wc <= 0x0B39))
    {
      if ((wc >= 0x0915 && wc <= 0x0939) ||
	  (wc >= 0x0958 && wc <= 0x095F) ||
	  (wc >= 0x0978 && wc <= 0x097F) || (wc >= 0x0995 && wc <= 0x09A8))
	return 1;
      if ((wc >= 0x09AA && wc <= 0x09DF))
	{
	  if ((wc >= 0x09AA && wc <= 0x09B0))
	    return 1;
	  if (wc == 0x09B2)
	    return 1;
	  if ((wc >= 0x09B6 && wc <= 0x09B9))
	    return 1;
	  if ((wc >= 0x09DC && wc <= 0x09DD) || wc == 0x09DF)
	    return 1;
	  return 0;
	}
      if ((wc >= 0x09F0 && wc <= 0x09F1) ||
	  (wc >= 0x0A95 && wc <= 0x0AA8) ||
	  (wc >= 0x0AAA && wc <= 0x0AB0) || (wc >= 0x0AB2 && wc <= 0x0AB3))
	return 1;
      if ((wc >= 0x0AB5 && wc <= 0x0B39))
	{
	  if ((wc >= 0x0AB5 && wc <= 0x0AB9))
	    return 1;
	  if (wc == 0x0AF9 || (wc >= 0x0B15 && wc <= 0x0B28))
	    return 1;
	  if ((wc >= 0x0B2A && wc <= 0x0B30))
	    return 1;
	  if ((wc >= 0x0B32 && wc <= 0x0B33) ||
	      (wc >= 0x0B35 && wc <= 0x0B39))
	    return 1;
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x0B5C && wc <= 0x1B0C))
    {
      if ((wc >= 0x0B5C && wc <= 0x0B5D) ||
	  wc == 0x0B5F || wc == 0x0B71 || (wc >= 0x0C15 && wc <= 0x0C28))
	return 1;
      if ((wc >= 0x0C2A && wc <= 0x103F))
	{
	  if ((wc >= 0x0C2A && wc <= 0x0C39))
	    return 1;
	  if ((wc >= 0x0C58 && wc <= 0x0C5A))
	    return 1;
	  if ((wc >= 0x0D15 && wc <= 0x0D3A))
	    return 1;
	  if ((wc >= 0x1000 && wc <= 0x102A) || wc == 0x103F)
	    return 1;
	  return 0;
	}
      if ((wc >= 0x1050 && wc <= 0x1055) ||
	  (wc >= 0x105A && wc <= 0x105D) ||
	  wc == 0x1061 || (wc >= 0x1065 && wc <= 0x1066))
	return 1;
      if ((wc >= 0x106E && wc <= 0x1B0C))
	{
	  if ((wc >= 0x106E && wc <= 0x1070))
	    return 1;
	  if ((wc >= 0x1075 && wc <= 0x1081) || wc == 0x108E)
	    return 1;
	  if ((wc >= 0x1780 && wc <= 0x17B3))
	    return 1;
	  if ((wc >= 0x1A20 && wc <= 0x1A54) ||
	      (wc >= 0x1B0B && wc <= 0x1B0C))
	    return 1;
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x1B13 && wc <= 0x10A17))
    {
      if ((wc >= 0x1B13 && wc <= 0x1B33) ||
	  (wc >= 0x1B45 && wc <= 0x1B4C) ||
	  (wc >= 0x1B83 && wc <= 0x1BA0) || (wc >= 0x1BAE && wc <= 0x1BAF))
	return 1;
      if ((wc >= 0x1BBB && wc <= 0xA9EF))
	{
	  if ((wc >= 0x1BBB && wc <= 0x1BBD))
	    return 1;
	  if ((wc >= 0xA989 && wc <= 0xA98B))
	    return 1;
	  if ((wc >= 0xA98F && wc <= 0xA9B2))
	    return 1;
	  if ((wc >= 0xA9E0 && wc <= 0xA9E4) ||
	      (wc >= 0xA9E7 && wc <= 0xA9EF))
	    return 1;
	  return 0;
	}
      if ((wc >= 0xA9FA && wc <= 0xA9FE) ||
	  (wc >= 0xAA60 && wc <= 0xAA6F) ||
	  (wc >= 0xAA71 && wc <= 0xAA73) || wc == 0xAA7A)
	return 1;
      if ((wc >= 0xAA7E && wc <= 0x10A17))
	{
	  if ((wc >= 0xAA7E && wc <= 0xAA7F))
	    return 1;
	  if ((wc >= 0xAAE0 && wc <= 0xAAEA) ||
	      (wc >= 0xABC0 && wc <= 0xABDA))
	    return 1;
	  if (wc == 0x10A00)
	    return 1;
	  if ((wc >= 0x10A10 && wc <= 0x10A13) ||
	      (wc >= 0x10A15 && wc <= 0x10A17))
	    return 1;
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x10A19 && wc <= 0x11F33))
    {
      if ((wc >= 0x10A19 && wc <= 0x10A35) ||
	  (wc >= 0x11103 && wc <= 0x11126) || wc == 0x11144 || wc == 0x11147)
	return 1;
      if ((wc >= 0x11380 && wc <= 0x11906))
	{
	  if ((wc >= 0x11380 && wc <= 0x11389))
	    return 1;
	  if (wc == 0x1138B)
	    return 1;
	  if (wc == 0x1138E)
	    return 1;
	  if ((wc >= 0x11390 && wc <= 0x113B5) ||
	      (wc >= 0x11900 && wc <= 0x11906))
	    return 1;
	  return 0;
	}
      if (wc == 0x11909 ||
	  (wc >= 0x1190C && wc <= 0x11913) ||
	  (wc >= 0x11915 && wc <= 0x11916) ||
	  (wc >= 0x11918 && wc <= 0x1192F))
	return 1;
      if ((wc >= 0x11A00 && wc <= 0x11F33))
	{
	  if (wc == 0x11A00)
	    return 1;
	  if ((wc >= 0x11A0B && wc <= 0x11A32) || wc == 0x11A50)
	    return 1;
	  if ((wc >= 0x11A5C && wc <= 0x11A83))
	    return 1;
	  if ((wc >= 0x11F04 && wc <= 0x11F10) ||
	      (wc >= 0x11F12 && wc <= 0x11F33))
	    return 1;
	  return 0;
	}
      return 0;
    }
  return 0;
}

static inline int
bui_is_Indic_Conjunct_Break_Extend (uint32_t wc)
{
  if ((wc >= 0x0300 && wc <= 0x0E3A))
    {
      if ((wc >= 0x0300 && wc <= 0x082D))
	{
	  if ((wc >= 0x0300 && wc <= 0x05C2))
	    {
	      if ((wc >= 0x0300 && wc <= 0x036F))
		return 1;
	      if ((wc >= 0x0483 && wc <= 0x0489))
		return 1;
	      if ((wc >= 0x0591 && wc <= 0x05BD))
		return 1;
	      if (wc == 0x05BF || (wc >= 0x05C1 && wc <= 0x05C2))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x05C4 && wc <= 0x06DC))
	    {
	      if ((wc >= 0x05C4 && wc <= 0x05C5))
		return 1;
	      if (wc == 0x05C7 || (wc >= 0x0610 && wc <= 0x061A))
		return 1;
	      if ((wc >= 0x064B && wc <= 0x065F))
		return 1;
	      if (wc == 0x0670 || (wc >= 0x06D6 && wc <= 0x06DC))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x06DF && wc <= 0x074A))
	    {
	      if ((wc >= 0x06DF && wc <= 0x06E4))
		return 1;
	      if ((wc >= 0x06E7 && wc <= 0x06E8))
		return 1;
	      if ((wc >= 0x06EA && wc <= 0x06ED))
		return 1;
	      if (wc == 0x0711 || (wc >= 0x0730 && wc <= 0x074A))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x07A6 && wc <= 0x082D))
	    {
	      if ((wc >= 0x07A6 && wc <= 0x07B0))
		return 1;
	      if ((wc >= 0x07EB && wc <= 0x07F3) || wc == 0x07FD)
		return 1;
	      if ((wc >= 0x0816 && wc <= 0x0819))
		return 1;
	      if ((wc >= 0x081B && wc <= 0x0823) ||
		  (wc >= 0x0825 && wc <= 0x0827) ||
		  (wc >= 0x0829 && wc <= 0x082D))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x0859 && wc <= 0x0A75))
	{
	  if ((wc >= 0x0859 && wc <= 0x093C))
	    {
	      if ((wc >= 0x0859 && wc <= 0x085B))
		return 1;
	      if ((wc >= 0x0897 && wc <= 0x089F) ||
		  (wc >= 0x08CA && wc <= 0x08E1))
		return 1;
	      if ((wc >= 0x08E3 && wc <= 0x0902))
		return 1;
	      if (wc == 0x093A || wc == 0x093C)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0941 && wc <= 0x09BE))
	    {
	      if ((wc >= 0x0941 && wc <= 0x0948))
		return 1;
	      if ((wc >= 0x0951 && wc <= 0x0957) ||
		  (wc >= 0x0962 && wc <= 0x0963))
		return 1;
	      if (wc == 0x0981)
		return 1;
	      if (wc == 0x09BC || wc == 0x09BE)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x09C1 && wc <= 0x0A3C))
	    {
	      if ((wc >= 0x09C1 && wc <= 0x09C4))
		return 1;
	      if (wc == 0x09D7 || (wc >= 0x09E2 && wc <= 0x09E3))
		return 1;
	      if (wc == 0x09FE)
		return 1;
	      if ((wc >= 0x0A01 && wc <= 0x0A02) || wc == 0x0A3C)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0A41 && wc <= 0x0A75))
	    {
	      if ((wc >= 0x0A41 && wc <= 0x0A42))
		return 1;
	      if ((wc >= 0x0A47 && wc <= 0x0A48) ||
		  (wc >= 0x0A4B && wc <= 0x0A4D))
		return 1;
	      if (wc == 0x0A51)
		return 1;
	      if ((wc >= 0x0A70 && wc <= 0x0A71) || wc == 0x0A75)
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x0A81 && wc <= 0x0C4C))
	{
	  if ((wc >= 0x0A81 && wc <= 0x0AE3))
	    {
	      if ((wc >= 0x0A81 && wc <= 0x0A82))
		return 1;
	      if (wc == 0x0ABC)
		return 1;
	      if ((wc >= 0x0AC1 && wc <= 0x0AC5))
		return 1;
	      if ((wc >= 0x0AC7 && wc <= 0x0AC8) ||
		  (wc >= 0x0AE2 && wc <= 0x0AE3))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0AFA && wc <= 0x0B57))
	    {
	      if ((wc >= 0x0AFA && wc <= 0x0AFF))
		return 1;
	      if (wc == 0x0B01 || wc == 0x0B3C)
		return 1;
	      if ((wc >= 0x0B3E && wc <= 0x0B3F))
		return 1;
	      if ((wc >= 0x0B41 && wc <= 0x0B44) ||
		  (wc >= 0x0B55 && wc <= 0x0B57))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0B62 && wc <= 0x0BCD))
	    {
	      if ((wc >= 0x0B62 && wc <= 0x0B63))
		return 1;
	      if (wc == 0x0B82)
		return 1;
	      if (wc == 0x0BBE)
		return 1;
	      if (wc == 0x0BC0 || wc == 0x0BCD)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0BD7 && wc <= 0x0C4C))
	    {
	      if (wc == 0x0BD7)
		return 1;
	      if (wc == 0x0C00 || wc == 0x0C04)
		return 1;
	      if (wc == 0x0C3C)
		return 1;
	      if ((wc >= 0x0C3E && wc <= 0x0C40) ||
		  (wc >= 0x0C46 && wc <= 0x0C48) ||
		  (wc >= 0x0C4A && wc <= 0x0C4C))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x0C55 && wc <= 0x0E3A))
	{
	  if ((wc >= 0x0C55 && wc <= 0x0CC2))
	    {
	      if ((wc >= 0x0C55 && wc <= 0x0C56))
		return 1;
	      if ((wc >= 0x0C62 && wc <= 0x0C63) || wc == 0x0C81)
		return 1;
	      if (wc == 0x0CBC)
		return 1;
	      if ((wc >= 0x0CBF && wc <= 0x0CC0) || wc == 0x0CC2)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0CC6 && wc <= 0x0D3C))
	    {
	      if ((wc >= 0x0CC6 && wc <= 0x0CC8))
		return 1;
	      if ((wc >= 0x0CCA && wc <= 0x0CCD) ||
		  (wc >= 0x0CD5 && wc <= 0x0CD6))
		return 1;
	      if ((wc >= 0x0CE2 && wc <= 0x0CE3))
		return 1;
	      if ((wc >= 0x0D00 && wc <= 0x0D01) ||
		  (wc >= 0x0D3B && wc <= 0x0D3C))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0D3E && wc <= 0x0DCA))
	    {
	      if (wc == 0x0D3E)
		return 1;
	      if ((wc >= 0x0D41 && wc <= 0x0D44) || wc == 0x0D57)
		return 1;
	      if ((wc >= 0x0D62 && wc <= 0x0D63))
		return 1;
	      if (wc == 0x0D81 || wc == 0x0DCA)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0DCF && wc <= 0x0E3A))
	    {
	      if (wc == 0x0DCF)
		return 1;
	      if ((wc >= 0x0DD2 && wc <= 0x0DD4) || wc == 0x0DD6)
		return 1;
	      if (wc == 0x0DDF)
		return 1;
	      if (wc == 0x0E31 || (wc >= 0x0E34 && wc <= 0x0E3A))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x0E47 && wc <= 0xA8FF))
    {
      if ((wc >= 0x0E47 && wc <= 0x1086))
	{
	  if ((wc >= 0x0E47 && wc <= 0x0F19))
	    {
	      if ((wc >= 0x0E47 && wc <= 0x0E4E))
		return 1;
	      if (wc == 0x0EB1)
		return 1;
	      if ((wc >= 0x0EB4 && wc <= 0x0EBC))
		return 1;
	      if ((wc >= 0x0EC8 && wc <= 0x0ECE) ||
		  (wc >= 0x0F18 && wc <= 0x0F19))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0F35 && wc <= 0x0F87))
	    {
	      if (wc == 0x0F35)
		return 1;
	      if (wc == 0x0F37 || wc == 0x0F39)
		return 1;
	      if ((wc >= 0x0F71 && wc <= 0x0F7E))
		return 1;
	      if ((wc >= 0x0F80 && wc <= 0x0F84) ||
		  (wc >= 0x0F86 && wc <= 0x0F87))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x0F8D && wc <= 0x1037))
	    {
	      if ((wc >= 0x0F8D && wc <= 0x0F97))
		return 1;
	      if ((wc >= 0x0F99 && wc <= 0x0FBC))
		return 1;
	      if (wc == 0x0FC6)
		return 1;
	      if ((wc >= 0x102D && wc <= 0x1030) ||
		  (wc >= 0x1032 && wc <= 0x1037))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x103A && wc <= 0x1086))
	    {
	      if (wc == 0x103A)
		return 1;
	      if ((wc >= 0x103D && wc <= 0x103E) ||
		  (wc >= 0x1058 && wc <= 0x1059))
		return 1;
	      if ((wc >= 0x105E && wc <= 0x1060))
		return 1;
	      if ((wc >= 0x1071 && wc <= 0x1074) ||
		  wc == 0x1082 || (wc >= 0x1085 && wc <= 0x1086))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x108D && wc <= 0x1A56))
	{
	  if ((wc >= 0x108D && wc <= 0x1753))
	    {
	      if (wc == 0x108D)
		return 1;
	      if (wc == 0x109D || (wc >= 0x135D && wc <= 0x135F))
		return 1;
	      if ((wc >= 0x1712 && wc <= 0x1715))
		return 1;
	      if ((wc >= 0x1732 && wc <= 0x1734) ||
		  (wc >= 0x1752 && wc <= 0x1753))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1772 && wc <= 0x17D3))
	    {
	      if ((wc >= 0x1772 && wc <= 0x1773))
		return 1;
	      if ((wc >= 0x17B4 && wc <= 0x17B5) ||
		  (wc >= 0x17B7 && wc <= 0x17BD))
		return 1;
	      if (wc == 0x17C6)
		return 1;
	      if ((wc >= 0x17C9 && wc <= 0x17D1) || wc == 0x17D3)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x17DD && wc <= 0x1922))
	    {
	      if (wc == 0x17DD)
		return 1;
	      if ((wc >= 0x180B && wc <= 0x180D) || wc == 0x180F)
		return 1;
	      if ((wc >= 0x1885 && wc <= 0x1886))
		return 1;
	      if (wc == 0x18A9 || (wc >= 0x1920 && wc <= 0x1922))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1927 && wc <= 0x1A56))
	    {
	      if ((wc >= 0x1927 && wc <= 0x1928))
		return 1;
	      if (wc == 0x1932 || (wc >= 0x1939 && wc <= 0x193B))
		return 1;
	      if ((wc >= 0x1A17 && wc <= 0x1A18))
		return 1;
	      if (wc == 0x1A1B || wc == 0x1A56)
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x1A58 && wc <= 0x1CE0))
	{
	  if ((wc >= 0x1A58 && wc <= 0x1A7F))
	    {
	      if ((wc >= 0x1A58 && wc <= 0x1A5E))
		return 1;
	      if (wc == 0x1A62)
		return 1;
	      if ((wc >= 0x1A65 && wc <= 0x1A6C))
		return 1;
	      if ((wc >= 0x1A73 && wc <= 0x1A7C) || wc == 0x1A7F)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1AB0 && wc <= 0x1B73))
	    {
	      if ((wc >= 0x1AB0 && wc <= 0x1ADD))
		return 1;
	      if ((wc >= 0x1AE0 && wc <= 0x1AEB) ||
		  (wc >= 0x1B00 && wc <= 0x1B03))
		return 1;
	      if ((wc >= 0x1B34 && wc <= 0x1B3D))
		return 1;
	      if ((wc >= 0x1B42 && wc <= 0x1B43) ||
		  (wc >= 0x1B6B && wc <= 0x1B73))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1B80 && wc <= 0x1BE6))
	    {
	      if ((wc >= 0x1B80 && wc <= 0x1B81))
		return 1;
	      if ((wc >= 0x1BA2 && wc <= 0x1BA5))
		return 1;
	      if ((wc >= 0x1BA8 && wc <= 0x1BAA))
		return 1;
	      if ((wc >= 0x1BAC && wc <= 0x1BAD) || wc == 0x1BE6)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1BE8 && wc <= 0x1CE0))
	    {
	      if ((wc >= 0x1BE8 && wc <= 0x1BE9))
		return 1;
	      if (wc == 0x1BED || (wc >= 0x1BEF && wc <= 0x1BF3))
		return 1;
	      if ((wc >= 0x1C2C && wc <= 0x1C33))
		return 1;
	      if ((wc >= 0x1C36 && wc <= 0x1C37) ||
		  (wc >= 0x1CD0 && wc <= 0x1CD2) ||
		  (wc >= 0x1CD4 && wc <= 0x1CE0))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x1CE2 && wc <= 0xA8FF))
	{
	  if ((wc >= 0x1CE2 && wc <= 0x200D))
	    {
	      if ((wc >= 0x1CE2 && wc <= 0x1CE8))
		return 1;
	      if (wc == 0x1CED || wc == 0x1CF4)
		return 1;
	      if ((wc >= 0x1CF8 && wc <= 0x1CF9))
		return 1;
	      if ((wc >= 0x1DC0 && wc <= 0x1DFF) || wc == 0x200D)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x20D0 && wc <= 0x309A))
	    {
	      if ((wc >= 0x20D0 && wc <= 0x20F0))
		return 1;
	      if ((wc >= 0x2CEF && wc <= 0x2CF1) || wc == 0x2D7F)
		return 1;
	      if ((wc >= 0x2DE0 && wc <= 0x2DFF))
		return 1;
	      if ((wc >= 0x302A && wc <= 0x302F) ||
		  (wc >= 0x3099 && wc <= 0x309A))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0xA66F && wc <= 0xA806))
	    {
	      if ((wc >= 0xA66F && wc <= 0xA672))
		return 1;
	      if ((wc >= 0xA674 && wc <= 0xA67D) ||
		  (wc >= 0xA69E && wc <= 0xA69F))
		return 1;
	      if ((wc >= 0xA6F0 && wc <= 0xA6F1))
		return 1;
	      if (wc == 0xA802 || wc == 0xA806)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0xA80B && wc <= 0xA8FF))
	    {
	      if (wc == 0xA80B)
		return 1;
	      if ((wc >= 0xA825 && wc <= 0xA826) || wc == 0xA82C)
		return 1;
	      if ((wc >= 0xA8C4 && wc <= 0xA8C5))
		return 1;
	      if ((wc >= 0xA8E0 && wc <= 0xA8F1) || wc == 0xA8FF)
		return 1;
	      return 0;
	    }
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0xA926 && wc <= 0x115BD))
    {
      if ((wc >= 0xA926 && wc <= 0xABED))
	{
	  if ((wc >= 0xA926 && wc <= 0xA9B3))
	    {
	      if ((wc >= 0xA926 && wc <= 0xA92D))
		return 1;
	      if ((wc >= 0xA947 && wc <= 0xA951))
		return 1;
	      if (wc == 0xA953)
		return 1;
	      if ((wc >= 0xA980 && wc <= 0xA982) || wc == 0xA9B3)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0xA9B6 && wc <= 0xAA36))
	    {
	      if ((wc >= 0xA9B6 && wc <= 0xA9B9))
		return 1;
	      if ((wc >= 0xA9BC && wc <= 0xA9BD) || wc == 0xA9E5)
		return 1;
	      if ((wc >= 0xAA29 && wc <= 0xAA2E))
		return 1;
	      if ((wc >= 0xAA31 && wc <= 0xAA32) ||
		  (wc >= 0xAA35 && wc <= 0xAA36))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0xAA43 && wc <= 0xAAB4))
	    {
	      if (wc == 0xAA43)
		return 1;
	      if (wc == 0xAA4C)
		return 1;
	      if (wc == 0xAA7C)
		return 1;
	      if (wc == 0xAAB0 || (wc >= 0xAAB2 && wc <= 0xAAB4))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0xAAB7 && wc <= 0xABED))
	    {
	      if ((wc >= 0xAAB7 && wc <= 0xAAB8))
		return 1;
	      if ((wc >= 0xAABE && wc <= 0xAABF) || wc == 0xAAC1)
		return 1;
	      if ((wc >= 0xAAEC && wc <= 0xAAED))
		return 1;
	      if (wc == 0xABE5 || wc == 0xABE8 || wc == 0xABED)
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0xFB1E && wc <= 0x110B6))
	{
	  if ((wc >= 0xFB1E && wc <= 0x102E0))
	    {
	      if (wc == 0xFB1E)
		return 1;
	      if ((wc >= 0xFE00 && wc <= 0xFE0F) ||
		  (wc >= 0xFE20 && wc <= 0xFE2F))
		return 1;
	      if ((wc >= 0xFF9E && wc <= 0xFF9F))
		return 1;
	      if (wc == 0x101FD || wc == 0x102E0)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x10376 && wc <= 0x10AE6))
	    {
	      if ((wc >= 0x10376 && wc <= 0x1037A))
		return 1;
	      if ((wc >= 0x10A01 && wc <= 0x10A03) ||
		  (wc >= 0x10A05 && wc <= 0x10A06))
		return 1;
	      if ((wc >= 0x10A0C && wc <= 0x10A0F))
		return 1;
	      if ((wc >= 0x10A38 && wc <= 0x10A3A) ||
		  (wc >= 0x10AE5 && wc <= 0x10AE6))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x10D24 && wc <= 0x10F85))
	    {
	      if ((wc >= 0x10D24 && wc <= 0x10D27))
		return 1;
	      if ((wc >= 0x10D69 && wc <= 0x10D6D) ||
		  (wc >= 0x10EAB && wc <= 0x10EAC))
		return 1;
	      if ((wc >= 0x10EFA && wc <= 0x10EFF))
		return 1;
	      if ((wc >= 0x10F46 && wc <= 0x10F50) ||
		  (wc >= 0x10F82 && wc <= 0x10F85))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11001 && wc <= 0x110B6))
	    {
	      if (wc == 0x11001)
		return 1;
	      if ((wc >= 0x11038 && wc <= 0x11046) || wc == 0x11070)
		return 1;
	      if ((wc >= 0x11073 && wc <= 0x11074))
		return 1;
	      if ((wc >= 0x1107F && wc <= 0x11081) ||
		  (wc >= 0x110B3 && wc <= 0x110B6))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x110B9 && wc <= 0x1134D))
	{
	  if ((wc >= 0x110B9 && wc <= 0x11132))
	    {
	      if ((wc >= 0x110B9 && wc <= 0x110BA))
		return 1;
	      if (wc == 0x110C2)
		return 1;
	      if ((wc >= 0x11100 && wc <= 0x11102))
		return 1;
	      if ((wc >= 0x11127 && wc <= 0x1112B) ||
		  (wc >= 0x1112D && wc <= 0x11132))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11134 && wc <= 0x111CC))
	    {
	      if (wc == 0x11134)
		return 1;
	      if (wc == 0x11173 || (wc >= 0x11180 && wc <= 0x11181))
		return 1;
	      if ((wc >= 0x111B6 && wc <= 0x111BE))
		return 1;
	      if (wc == 0x111C0 || (wc >= 0x111C9 && wc <= 0x111CC))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x111CF && wc <= 0x11241))
	    {
	      if (wc == 0x111CF)
		return 1;
	      if ((wc >= 0x1122F && wc <= 0x11231))
		return 1;
	      if ((wc >= 0x11234 && wc <= 0x11237))
		return 1;
	      if (wc == 0x1123E || wc == 0x11241)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x112DF && wc <= 0x1134D))
	    {
	      if (wc == 0x112DF)
		return 1;
	      if ((wc >= 0x112E3 && wc <= 0x112EA) ||
		  (wc >= 0x11300 && wc <= 0x11301))
		return 1;
	      if ((wc >= 0x1133B && wc <= 0x1133C))
		return 1;
	      if (wc == 0x1133E || wc == 0x11340 || wc == 0x1134D)
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x11357 && wc <= 0x115BD))
	{
	  if ((wc >= 0x11357 && wc <= 0x113C2))
	    {
	      if (wc == 0x11357)
		return 1;
	      if ((wc >= 0x11366 && wc <= 0x1136C) ||
		  (wc >= 0x11370 && wc <= 0x11374))
		return 1;
	      if (wc == 0x113B8)
		return 1;
	      if ((wc >= 0x113BB && wc <= 0x113C0) || wc == 0x113C2)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x113C5 && wc <= 0x1143F))
	    {
	      if (wc == 0x113C5)
		return 1;
	      if ((wc >= 0x113C7 && wc <= 0x113C9) ||
		  (wc >= 0x113CE && wc <= 0x113CF))
		return 1;
	      if (wc == 0x113D2)
		return 1;
	      if ((wc >= 0x113E1 && wc <= 0x113E2) ||
		  (wc >= 0x11438 && wc <= 0x1143F))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11442 && wc <= 0x114BA))
	    {
	      if ((wc >= 0x11442 && wc <= 0x11444))
		return 1;
	      if (wc == 0x11446 || wc == 0x1145E)
		return 1;
	      if (wc == 0x114B0)
		return 1;
	      if ((wc >= 0x114B3 && wc <= 0x114B8) || wc == 0x114BA)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x114BD && wc <= 0x115BD))
	    {
	      if (wc == 0x114BD)
		return 1;
	      if ((wc >= 0x114BF && wc <= 0x114C0) ||
		  (wc >= 0x114C2 && wc <= 0x114C3))
		return 1;
	      if (wc == 0x115AF)
		return 1;
	      if ((wc >= 0x115B2 && wc <= 0x115B5) ||
		  (wc >= 0x115BC && wc <= 0x115BD))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      return 0;
    }
  if ((wc >= 0x115BF && wc <= 0xE01EF))
    {
      if ((wc >= 0x115BF && wc <= 0x11A3E))
	{
	  if ((wc >= 0x115BF && wc <= 0x11640))
	    {
	      if ((wc >= 0x115BF && wc <= 0x115C0))
		return 1;
	      if ((wc >= 0x115DC && wc <= 0x115DD))
		return 1;
	      if ((wc >= 0x11633 && wc <= 0x1163A))
		return 1;
	      if (wc == 0x1163D || (wc >= 0x1163F && wc <= 0x11640))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x116AB && wc <= 0x11725))
	    {
	      if (wc == 0x116AB)
		return 1;
	      if (wc == 0x116AD || (wc >= 0x116B0 && wc <= 0x116B7))
		return 1;
	      if (wc == 0x1171D)
		return 1;
	      if (wc == 0x1171F || (wc >= 0x11722 && wc <= 0x11725))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11727 && wc <= 0x1193D))
	    {
	      if ((wc >= 0x11727 && wc <= 0x1172B))
		return 1;
	      if ((wc >= 0x1182F && wc <= 0x11837))
		return 1;
	      if ((wc >= 0x11839 && wc <= 0x1183A))
		return 1;
	      if (wc == 0x11930 || (wc >= 0x1193B && wc <= 0x1193D))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11943 && wc <= 0x11A3E))
	    {
	      if (wc == 0x11943)
		return 1;
	      if ((wc >= 0x119D4 && wc <= 0x119D7) ||
		  (wc >= 0x119DA && wc <= 0x119DB))
		return 1;
	      if (wc == 0x119E0)
		return 1;
	      if ((wc >= 0x11A01 && wc <= 0x11A0A) ||
		  (wc >= 0x11A33 && wc <= 0x11A38) ||
		  (wc >= 0x11A3B && wc <= 0x11A3E))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x11A51 && wc <= 0x11F01))
	{
	  if ((wc >= 0x11A51 && wc <= 0x11B64))
	    {
	      if ((wc >= 0x11A51 && wc <= 0x11A56))
		return 1;
	      if ((wc >= 0x11A59 && wc <= 0x11A5B) ||
		  (wc >= 0x11A8A && wc <= 0x11A96))
		return 1;
	      if (wc == 0x11A98)
		return 1;
	      if (wc == 0x11B60 || (wc >= 0x11B62 && wc <= 0x11B64))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11B66 && wc <= 0x11CB0))
	    {
	      if (wc == 0x11B66)
		return 1;
	      if ((wc >= 0x11C30 && wc <= 0x11C36) ||
		  (wc >= 0x11C38 && wc <= 0x11C3D))
		return 1;
	      if (wc == 0x11C3F)
		return 1;
	      if ((wc >= 0x11C92 && wc <= 0x11CA7) ||
		  (wc >= 0x11CAA && wc <= 0x11CB0))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11CB2 && wc <= 0x11D45))
	    {
	      if ((wc >= 0x11CB2 && wc <= 0x11CB3))
		return 1;
	      if ((wc >= 0x11CB5 && wc <= 0x11CB6) ||
		  (wc >= 0x11D31 && wc <= 0x11D36))
		return 1;
	      if (wc == 0x11D3A)
		return 1;
	      if ((wc >= 0x11D3C && wc <= 0x11D3D) ||
		  (wc >= 0x11D3F && wc <= 0x11D45))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x11D47 && wc <= 0x11F01))
	    {
	      if (wc == 0x11D47)
		return 1;
	      if ((wc >= 0x11D90 && wc <= 0x11D91) || wc == 0x11D95)
		return 1;
	      if (wc == 0x11D97)
		return 1;
	      if ((wc >= 0x11EF3 && wc <= 0x11EF4) ||
		  (wc >= 0x11F00 && wc <= 0x11F01))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x11F36 && wc <= 0x1DA36))
	{
	  if ((wc >= 0x11F36 && wc <= 0x13455))
	    {
	      if ((wc >= 0x11F36 && wc <= 0x11F3A))
		return 1;
	      if ((wc >= 0x11F40 && wc <= 0x11F41))
		return 1;
	      if (wc == 0x11F5A)
		return 1;
	      if (wc == 0x13440 || (wc >= 0x13447 && wc <= 0x13455))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1611E && wc <= 0x16F92))
	    {
	      if ((wc >= 0x1611E && wc <= 0x16129))
		return 1;
	      if ((wc >= 0x1612D && wc <= 0x1612F) ||
		  (wc >= 0x16AF0 && wc <= 0x16AF4))
		return 1;
	      if ((wc >= 0x16B30 && wc <= 0x16B36))
		return 1;
	      if (wc == 0x16F4F || (wc >= 0x16F8F && wc <= 0x16F92))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x16FE4 && wc <= 0x1CF46))
	    {
	      if (wc == 0x16FE4)
		return 1;
	      if ((wc >= 0x16FF0 && wc <= 0x16FF1))
		return 1;
	      if ((wc >= 0x1BC9D && wc <= 0x1BC9E))
		return 1;
	      if ((wc >= 0x1CF00 && wc <= 0x1CF2D) ||
		  (wc >= 0x1CF30 && wc <= 0x1CF46))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1D165 && wc <= 0x1DA36))
	    {
	      if ((wc >= 0x1D165 && wc <= 0x1D169))
		return 1;
	      if ((wc >= 0x1D16D && wc <= 0x1D172) ||
		  (wc >= 0x1D17B && wc <= 0x1D182))
		return 1;
	      if ((wc >= 0x1D185 && wc <= 0x1D18B))
		return 1;
	      if ((wc >= 0x1D1AA && wc <= 0x1D1AD) ||
		  (wc >= 0x1D242 && wc <= 0x1D244) ||
		  (wc >= 0x1DA00 && wc <= 0x1DA36))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      if ((wc >= 0x1DA3B && wc <= 0xE01EF))
	{
	  if ((wc >= 0x1DA3B && wc <= 0x1E006))
	    {
	      if ((wc >= 0x1DA3B && wc <= 0x1DA6C))
		return 1;
	      if (wc == 0x1DA75 || wc == 0x1DA84)
		return 1;
	      if ((wc >= 0x1DA9B && wc <= 0x1DA9F))
		return 1;
	      if ((wc >= 0x1DAA1 && wc <= 0x1DAAF) ||
		  (wc >= 0x1E000 && wc <= 0x1E006))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1E008 && wc <= 0x1E136))
	    {
	      if ((wc >= 0x1E008 && wc <= 0x1E018))
		return 1;
	      if ((wc >= 0x1E01B && wc <= 0x1E021) ||
		  (wc >= 0x1E023 && wc <= 0x1E024))
		return 1;
	      if ((wc >= 0x1E026 && wc <= 0x1E02A))
		return 1;
	      if (wc == 0x1E08F || (wc >= 0x1E130 && wc <= 0x1E136))
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1E2AE && wc <= 0x1E6E6))
	    {
	      if (wc == 0x1E2AE)
		return 1;
	      if ((wc >= 0x1E2EC && wc <= 0x1E2EF) ||
		  (wc >= 0x1E4EC && wc <= 0x1E4EF))
		return 1;
	      if ((wc >= 0x1E5EE && wc <= 0x1E5EF))
		return 1;
	      if (wc == 0x1E6E3 || wc == 0x1E6E6)
		return 1;
	      return 0;
	    }
	  if ((wc >= 0x1E6EE && wc <= 0xE01EF))
	    {
	      if ((wc >= 0x1E6EE && wc <= 0x1E6EF))
		return 1;
	      if (wc == 0x1E6F5 || (wc >= 0x1E8D0 && wc <= 0x1E8D6))
		return 1;
	      if ((wc >= 0x1E944 && wc <= 0x1E94A))
		return 1;
	      if ((wc >= 0x1F3FB && wc <= 0x1F3FF) ||
		  (wc >= 0xE0020 && wc <= 0xE007F) ||
		  (wc >= 0xE0100 && wc <= 0xE01EF))
		return 1;
	      return 0;
	    }
	  return 0;
	}
      return 0;
    }
  return 0;
}

#endif /* BUI_BREAK_PROPS_H */
