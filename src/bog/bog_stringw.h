#ifndef BOG_STRINGW_H
#define BOG_STRINGW_H

static bool BOGIsDigitW(wchar_t c) {
	return (c >= L'0' && c <= L'9');
}

static bool BOGIsNumberW(wchar_t *s) {
	wchar_t *temp = s;
	if (temp[0] == L'-' && temp[1]) temp++;
	while (*temp && BOGIsDigitW(*temp)) temp++;
	return !*temp;
}

static size_t BOGStringLengthW(wchar_t *s) {
	wchar_t *p = s;
	while (*s) s++;
    
	return (s - p);
}

static wchar_t * BOGStringCatW(wchar_t *dest, wchar_t *src) {
	wchar_t *temp = dest;
	while (*dest) dest++;
	while (*dest++ = *src++);
    
	return temp;
}

static int32_t BOGStringCompareW(wchar_t *s1, wchar_t *s2) {
	while(*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
    
	return (*s1 - *s2);
}

static wchar_t * BOGStringCopyW(wchar_t *dest, wchar_t *src) {
	wchar_t *temp = dest;
	while (*dest++ = *src++);
    
	return temp;
}

static wchar_t * BOGStringCatToCharW(wchar_t *dest, wchar_t *src, wchar_t c) {
	wchar_t *temp = dest;
	while (*dest) dest++;
	while ((*src != c) && (*dest++ = *src++));
	
	return temp;
}

static wchar_t * BOGStringCatToIndexW(wchar_t *dest, wchar_t *src, int32_t index) {
	wchar_t *temp = dest;
	while (*dest) dest++;
	while (index-- && (*dest++ = *src++));
	
	return temp;
}

static wchar_t * BOGStringCatFromCharW(wchar_t *dest, wchar_t *src, wchar_t c) {
	while (*src && (*src != c)) *src++;
	
	return BOGStringCatW(dest, src);
}

static wchar_t * BOGStringCatFromIndexW(wchar_t *dest, wchar_t *src, int32_t index) {
	while (*src && index--) *src++;
	
	return BOGStringCatW(dest, src);
}

static int32_t BOGStringCompareToCharW(wchar_t *s1, wchar_t *s2, wchar_t c) {
	while (*s1 && (*s1 == *s2) && (*s1 != c)) {
		s1++;
		s2++;
	}
	
	return (*s1 - *s2);
}

static int32_t BOGStringCompareToIndexW(wchar_t *s1, wchar_t *s2, int32_t index) {
	while (*s1 && (*s1 == *s2) && --index) {
		s1++;
		s2++;
	}
	
	return (*s1 - *s2);
}

static int32_t BOGStringCompareFromCharW(wchar_t *s1, wchar_t *s2, wchar_t c) {
	while (*s1 && (*s1 != c)) s1++;
	while (*s2 && (*s2 != c)) s2++;
	
	return BOGStringCompareW(s1, s2);
}

static int32_t BOGStringCompareFromIndexW(wchar_t *s1, wchar_t *s2, int32_t index) {
	while ((*s1 && *s2) && --index) {
		s1++;
		s2++;
	}
	
	return BOGStringCompareW(s1, s2);
}

static wchar_t * BOGStringCopyToCharW(wchar_t *dest, wchar_t* src, wchar_t c) {
	wchar_t *temp = dest;
	while ((*src != c) && (*dest++ = *src++));
	
	return temp;
}

static wchar_t * BOGStringCopyToIndexW(wchar_t *dest, wchar_t *src, int32_t index) {
	wchar_t *temp = dest;
	while (index-- && (*dest++ = *src++));
	
	return temp;
}

static wchar_t * BOGStringCopyFromCharW(wchar_t *dest, wchar_t *src, wchar_t c) {
	while (*src && *src != c) src++;
	
	return BOGStringCopyW(dest, src);
}

static wchar_t * BOGStringCopyFromIndexW(wchar_t *dest, wchar_t *src, int32_t index) {
	while (*src && index--) src++;
	
	return BOGStringCopyW(dest, src);
}

static size_t BOGStringLengthToCharW(wchar_t *s, wchar_t c) {
	wchar_t *p = s;
	while (*s && (*s != c)) s++;
	
	return (s - p);
}

static size_t BOGStringLengthToIndexW(wchar_t *s, int32_t index) {
	wchar_t *p = s;
	while (*s && index--) s++;
	
	return (s - p);
}

static size_t BOGStringLengthFromCharW(wchar_t *s, wchar_t c) {
	while (*s && (*s != c)) s++;
	
	return BOGStringLengthW(s);
}

static size_t BOGStringLengthFromIndexW(wchar_t *s, int32_t index) {
	while (*s && index--) s++;
	
	return BOGStringLengthW(s);
}

static wchar_t * BOGStringReverseW(wchar_t *s) {
	size_t len = BOGStringLengthW(s);
	for (size_t i = 0; i < len / 2; ++i) {
		wchar_t start = s[i];
		wchar_t end = s[(len - 1) - i];
		s[(len - 1) - i] = start;
		s[i] = end;
	}
	
	return s;
}

static wchar_t * BOGStringReverseToCharW(wchar_t *s, wchar_t c) {
	size_t len = 0;
	while (*s && *s != c) len++;
	
	for (size_t i = 0; i < len / 2; ++i) {
		wchar_t start = s[i];
		wchar_t end = s[(len - 1) - i];
		s[(len - 1) - i] = start;
		s[i] = end;
	}
	
	return s;
}

static wchar_t * BOGStringReverseToIndexW(wchar_t *s, int32_t index) {
	size_t len = 0;
	while (*s && index--) len++;
	
	for (size_t i = 0; i < len / 2; ++i) {
		wchar_t start = s[i];
		wchar_t end = s[(len - 1) - i];
		s[(len - 1) - i] = start;
		s[i] = end;
	}
	
	return s;
}

static wchar_t * BOGStringReverseFromCharW(wchar_t *s, wchar_t c) {
	wchar_t *temp = s;
	while (*s && *s != c) s++;
	
	size_t len = BOGStringLengthW(temp);
	for (size_t i = 0; i < len / 2; ++i) {
		wchar_t start = s[i];
		wchar_t end = s[(len - 1) - i];
		s[(len - 1) - i] = start;
		s[i] = end;
	}
	
	return temp;
}

static wchar_t * BOGStringReverseFromIndexW(wchar_t *s, int32_t index) {
	wchar_t *temp = s;
	while (*s && index--) s++;
	
	size_t len = BOGStringLengthW(temp);
	for (size_t i = 0; i < len / 2; ++i) {
		wchar_t start = s[i];
		wchar_t end = s[(len - 1) - i];
		s[(len - 1) - i] = start;
		s[i] = end;
	}
	
	return temp;
}

static wchar_t * BOGStringContainsW(wchar_t *haystack, wchar_t *needle) {
	size_t len = BOGStringLengthW(needle);
	
	while (*haystack) {
		while (!BOGStringCompareToIndexW(haystack++, needle, (int32_t) len)) return (haystack - 1);
	}
	
	return 0;
}

int32_t BOGStringToIntW(wchar_t *s) {
	wchar_t *temp = s;
	int32_t result = 0;
	int8_t sign = 1;
	
	if (*s == '-') {
		sign = -1;
		*s++;
	}
	
	while (*temp && BOGIsNumberW(s)) result = 10 * result + (*temp++ - L'0');
	
	return sign * result;
}

#endif //BOG_STRINGW_H
