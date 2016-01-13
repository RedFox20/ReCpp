/**
 * String Tokenizer/View, Copyright (c) 2014 - Jorma Rebane
 */
#include "strview.h"

namespace rpp // ReCpp
{
	// This is same as memchr, but optimized for very small control strings
	FINLINE bool strcontains(const char* str, int len, char ch) {
		for (int i = 0; i < len; ++i) if (str[i] == ch) return true; // found it.
		return false;
	}
	/**
	 * @note Same as strpbrk, except we're not dealing with 0-term strings
	 * @note This function is optimized for 4-8 char str and 3-4 char control.
	 */
	FINLINE const char* strcontains(const char* str, int nstr, const char* control, int ncontrol) {
		for (auto s = str; nstr; --nstr, ++s)
			if (strcontains(control, ncontrol, *s)) return s; // done
		return 0; // not found
	}
	NOINLINE bool strequals(const char* s1, const char* s2, int len) {
		for (int i = 0; i < len; ++i) 
			if (s1[i] != s2[i]) return false; // not equal.
		return true;
	}
	NOINLINE bool strequalsi(const char* s1, const char* s2, int len) {
		for (int i = 0; i < len; ++i) 
			if (::toupper(s1[i]) != ::toupper(s2[i])) return false; // not equal.
		return true;
	}


	///////////// strview

	bool strview::to_bool() const
	{
		const uint len = this->len;
		if (len > 4)
			return false; // can't be any of true literals
		return (len == 4 && strequalsi(str, "true")) ||
			   (len == 3 && strequalsi(str, "yes"))  ||
			   (len == 2 && strequalsi(str, "on"))   ||
			   (len == 1 && strequalsi(str, "1"));
	}

	bool strview::is_whitespace()
	{
		auto* s = str, *e = s+len;
		for (; s < e && *(byte*)s <= ' '; ++s) ; // loop while is whitespace
		return s == e;
	}

	strview& strview::trim_start()
	{
		auto s = str;
		auto n = len;
		for (; n && *(byte*)s <= ' '; ++s, --n) ; // loop while is whitespace
		str = s, len = n; // result writeout
		return *this;
	}

	strview& strview::trim_start(char ch)
	{
		auto s = str;
		auto n = len;
		for (; n && *s == ch; ++s, --n) ;
		str = s, len = n; // result writeout
		return *this;
	}

	strview& strview::trim_start(const char* chars, int nchars)
	{
		auto s = str;
		auto n = len;
		for (; n && strcontains(chars, nchars, *s); ++s, --n);
		str = s, len = n; // result writeout
		return *this;
	}

	strview& strview::trim_end()
	{
		auto n = len;
		auto e = str + n;
		for (; n && *--e <= ' '; --n) ; // reverse loop while is whitespace
		len = n; // result writeout
		return *this;
	}

	strview& strview::trim_end(char ch)
	{
		auto n = len;
		auto e = str + n;
		for (; n && *--e == ch; --n) ;
		len = n; // result writeout
		return *this;
	}

	strview& strview::trim_end(const char* chars, int nchars)
	{
		auto n = len;
		auto e = str + n;
		for (; n && strcontains(chars, nchars, *--e); --n);
		len = n; // result writeout
		return *this;
	}

	strview strview::split_first(char delim)
	{
		if (auto splitend = (const char*)memchr(str, delim, len))
			return strview(str, splitend); // if we find a separator, readjust end of strview to that
		return strview(str, len);
	}

	strview strview::split_second(char delim)
	{
		if (auto splitstart = (const char*)memchr(str, delim, len))
			return strview(splitstart + 1, str+len); // readjust start, also skip the char we split at
		return strview(str, len);
	}



	int strview::split(std::vector<strview>& out, char delim, const char* trimChars)
	{
		int ntrims = trimChars ? (int)strlen(trimChars) : 0;
		int numSplits = 0;
		strview tok;
		strview splitter(str, len);
		while (splitter.next(tok, delim)) // get next strview
		{
			if (ntrims) tok.trim(trimChars, ntrims); // trim if needed
			if (tok.len) { // if we actually have anything after trimming?
				out.emplace_back(tok.str, tok.len); // push it out
				++numSplits;
			}
		}
		return numSplits;
	}
	int strview::split(std::vector<strview>& out, const char* delims, const char* trimChars)
	{
		int ntrims = trimChars ? (int)strlen(trimChars) : 0;
		int ndelims = (int)strlen(delims);
		int numSplits = 0;
		strview tok;
		strview splitter(str, len); // split source strview
		while (splitter.next(tok, delims, ndelims)) // get next strview
		{
			if (ntrims) tok.trim(trimChars, ntrims); // trim if needed
			if (tok.len) { // if we actually have anything after trimming?
				out.emplace_back(tok.str, tok.len); // push it out
				++numSplits;
			}
		}
		return numSplits;
	}



	bool strview::next(strview& out, char delim)
	{
		bool result = next_notrim(out, delim);
		if (result && len) ++str, --len; // trim match
		return result;
	}

	bool strview::next(strview& out, const char* delims, int ndelims)
	{
		bool result = next_notrim(out, delims, ndelims);
		if (result && len) ++str, --len; // trim match
		return result;
	}





	bool strview::next_notrim(strview& out, char delim)
	{
		return _next_notrim(out, [delim](const char* str, int len) {
			return (const char*)memchr(str, delim, len);
		});
	}

	bool strview::next_notrim(strview& out, const char* delims, int ndelims)
	{
		return _next_notrim(out, [delims, ndelims](const char* str, int len) {
			return strcontains(str, len, delims, ndelims);
		});
	}




	float strview::next_float()
	{
		auto s = str, e = s + len;
		for (; s < e; ++s) {
			char ch = *s; // check if we have the start of a number: "-0.25" || ".25" || "25":
			if (ch == '-' || ch == '.' || ('0' <= ch && ch <= '9')) {
				float f = _tofloat(s, &s);
				str = s, len = int(e - s);
				return f;
			}
		}
		str = s, len = int(e - s);
		return 0.0f;
	}

	int strview::next_int()
	{
		auto s = str, e = s + len;
		for (; s < e; ++s) {
			char ch = *s; // check if we have the start of a number: "-25" || "25":
			if (ch == '-' || ('0' <= ch && ch <= '9')) {
				int i = _toint(s, &s);
				str = s, len = int(e - s);
				return i;
			}
		}
		str = s, len = int(e - s);
		return 0;
	}

	void strview::skip_until(char ch)
	{
		auto s = str;
		auto n = len;
		for (; n && *s != ch; --n, ++s) ;
		str = s, len = n;
	}
	void strview::skip_until(const char* sstr, int slen)
	{
		auto s = str, e = s + len;
		char ch = *sstr; // starting char of the sequence
		while (s < e) {
			if (auto p = (const char*)memchr(s, ch, e - s)) {
				if (memcmp(p, sstr, slen) == 0) { // match found
					str = p, len = int(e - p);
					return;
				}
				s = p + 1;
				continue;
			}
			break;
		}
		str = e, len = 0;
	}

	void strview::skip_after(char ch)
	{
		skip_until(ch);
		if (len) ++str, --len; // skip after
	}
	void strview::skip_after(const char* sstr, int slen)
	{
		skip_until(sstr, slen);
		if (len) str += slen, len -= slen; // skip after
	}


	static FINLINE void foreach(char* str, int len, int func(int ch)) 
	{
		auto s = str, e = s + len;
		for (; s < e; ++s)
			*s = func(*s);
	}
	strview& strview::tolower() 
	{
		foreach((char*)str, len, ::tolower); return *this;
	}
	strview& strview::toupper()
	{
		foreach((char*)str, len, ::toupper); return *this;
	}
	char* strview::aslower(char* dst) const {
		auto p = dst;
		auto s = str;
		for (int n = len; n > 0; --n)
			*p++ = ::tolower(*s++);
		*p = 0;
		return dst;
	}
	char* strview::asupper(char* dst) const
	{
		auto p = dst;
		auto s = str;
		for (int n = len; n > 0; --n)
			*p++ = ::toupper(*s++);
		*p = 0;
		return dst;
	}
	std::string strview::aslower() const
	{
		std::string ret;
		ret.reserve(len);
		auto s = (char*)str;
		for (int n = len; n > 0; --n)
			ret.push_back(::tolower(*s++));
		return ret;
	}
	std::string strview::asupper() const
	{
		std::string ret;
		ret.reserve(len);
		auto s = (char*)str;
		for (int n = len; n > 0; --n)
			ret.push_back(::toupper(*s++));
		return ret;
	}




	char* tolower(char* str, int len) 
	{
		foreach(str, len, ::tolower); return str;
	}
	char* toupper(char* str, int len) 
	{
		foreach(str, len, ::toupper); return str;
	}
	std::string& tolower(std::string& str) 
	{
		foreach((char*)str.data(), (int)str.size(), ::tolower); return str;
	}
	std::string& toupper(std::string& str) 
	{
		foreach((char*)str.data(), (int)str.size(), ::toupper); return str;
	}

	char* replace(char* str, int len, char chOld, char chNew)
	{
		char* s = (char*)str, *e = s + len;
		for (; s < e; ++s) if (*s == chOld) *s = chNew;
		return str;
	}
	std::string& replace(std::string& str, char chOld, char chNew) {
		replace((char*)str.data(), (int)str.size(), chOld, chNew); return str;
	}


	strview& strview::replace(char chOld, char chNew)
	{
		auto s = (char*)str;
		for (int n = len; n > 0; --n, ++s)
			if (*s == chOld) *s = chNew;
		return *this;
	}





	////////////////////// loose utility functions

	/**
	 * @note Doesn't account for any special cases like +- NAN, E-9 etc. Just a very simple atof.
	 *       Some optimizations applied to avoid too many multiplications
	 *       This doesn't handle very big floats, max range is:
	 *           -2147483648.0f to 2147483647.0f, or -0.0000000001f to 0.0000000001f
	 *       Or the worst case:
	 *           214748.0001f
	 */
	float _tofloat(const char* str, const char** end)
	{
		int  power   = 1;
		int  intPart = _toint(str, &str);
		char ch      = *str;

		// @note The '.' is actually the sole reason for this function in the first place. Locale independence.
		if (ch == '.') { /* fraction part follows*/
			for (; '0' <= (ch = *++str) && ch <= '9'; ) {
				intPart = (intPart << 3) + (intPart << 1) + (ch - '0'); // intPart = intPart*10 + digit
				power   = (power << 3) + (power << 1); // power *= 10
			}
		}
		if (end) *end = str;
		return power == 1 ? float(intPart) : float(intPart) / float(power);
	}

	// optimized for simplicity and performance
	int _toint(const char* str, const char** end)
	{
		int  intPart  = 0;
		bool negative = false;
		char ch       = *str;

		if (ch == '-')
			negative = true, ch = *++str; // change sign and skip '-'
		do {
			intPart = (intPart << 3) + (intPart << 1) + (ch - '0'); // intPart = intPart*10 + digit
		} while ('0' <= (ch = *++str) && ch <= '9');
		if (negative) intPart = -intPart; // twiddle sign

		if (end) *end = str; // write end of parsed integer value
		return intPart;
	}

	// optimized for simplicity and performance
	// detects HEX integer strings as 0xBA or 0BA. Regular integers also parsed
	int _tointhx(const char* str, const char** end)
	{
		if (*str == '0') { // hex string?
			unsigned intPart = 0;
			if (str[1] == 'x') ++str;
			for (char ch;;) {
				unsigned digit;
				if ('0' <= (ch = *++str) && ch <= '9') digit = ch - '0';
				else if ('A' <= ch && ch <= 'F')       digit = ch - '7'; // hack 'A'-10 == '7'
				else if ('a' <= ch && ch <= 'f')       digit = ch - 'W'; // hack 'a'-10 == 'W'
				else break; // invalid ch
				intPart = (intPart << 4) + digit; // intPart = intPart*16 + digit
			}
			if (end) *end = str; // write end of parsed integer value
			return intPart;
		}
		return _toint(str, end); // regular int string
	}

	int _tostring(char* buffer, float f)
	{
		int value = (int)f;
		f -= value; // -1.2 -= -1 --> -0.2
		if (value < 0) f = -f;
		char* end = buffer + _tostring(buffer, value);

		if (f != 0.0f) { // do we have a fraction ?
			double cmp = 0.00001; // 6 decimal places max
			*end++ = '.'; // place the fraction mark
			double x = f; // floats go way imprecise when *=10, perhaps should extract mantissa instead?
			for (;;) {
				x *= 10;
				value = (int)x;
				*end++ = '0' + (value % 10);
				x -= value;
				if (x < cmp) // break from 0.750000011 cases
					break;
				cmp *= 10.0;
			}
		}
		*end = '\0'; // null-terminate
		return (int) (end - buffer); // length of the string
	}

	int _tostring(char* buffer, int value)
	{
		char* end = buffer;
		if (value < 0) { // if neg, abs and writeout '-'
			value = -value;	// flip sign
			*end++ = '-';	// neg
		}
		char* start = end; // mark start for strrev after:
	
		do { // writeout remainder of 10 + '0' while we still have value
			*end++ = '0' + (value % 10);
			value /= 10;
		} while (value != 0);
		*end = '\0'; // always null-terminate

		// reverse the string:
		char* rev = end; // for strrev, we'll need a helper pointer
		while (start < rev) {
			char tmp = *start;
			*start++ = *--rev;
			*rev = tmp;
		}

		return (int)(end - buffer); // length of the string
	}




	///////////// line_parser

	bool line_parser::read_line(strview& out)
	{
		while (buffer.next(out, '\n')) {
			out.trim_end("\n\r", 2); // trim off any newlines
			return true;
		}
		return false; // no more lines
	}

	strview line_parser::read_line()
	{
		strview out;
		while (buffer.next(out, '\n')) {
			out.trim_end("\n\r", 2); // trim off any newlines
			return out;
		}
		return out;
	}


	///////////// keyval_parser

	bool keyval_parser::read_line(strview& out)
	{
		strview line;
		while (buffer.next(line, '\n')) {
			// ignore comment lines or empty newlines
			if (line.str[0] == ';' || line.str[0] == '\n' || line.str[0] == '\r')
				continue; // skip to next line

			if (line.is_whitespace()) // is it all whitespace line?
				continue; // skip to next line

			// now we trim this beast and turn it into a c_str
			out = line.trim_start().split_first(';').trim_end();
			return true;
		}
		return false; // no more lines
	}
	bool keyval_parser::read_next(strview& key, strview& value)
	{
		strview line;
		while (read_line(line))
		{
			if (line.next(key, '=')) {
				key.trim();
				if (line.next(value, '=')) value.trim();
				else value.clear();
				return true;
			}
			return false;
		}
		return false;
	}


	///////////// bracket_parser

	bool bracket_parser::read_line(strview& key, strview& value, int& depth)
	{
		while (buffer.next(value, "\r\n", 2)) // split by carriage return
		{
			char ch = value.trim_start()[0];
			if (value.empty() || ch == ';') // ignore empty newlines and comments
				continue; // next line plz

			if (ch == '{') // increase depth level on opened bracket
			{ 
				++depth; 
				continue; 
			}
			if (ch == '}')
			{ 
				if (--depth <= 0) 
					return false; // } and depth dropped to <= 0, we're done with parsing this section
				continue; 
			}
		
			value.next(key, " \t;", 3);
			value.trim_start(" \t", 2); // trim any leading whitespace
			return true; // OK a value was read
		}
		return false; // no more lines in Buffer
	}
	void bracket_parser::skip_to_next(int& currentdepth, int targetdepth)
	{
		strview line;
		while (buffer.next(line, "\r\n", 2)) // split by carriage return
		{
			line = line.trim(); // trim the whole line
			if (line.empty()) // ignore empty newlines
				continue;

			char ch = line[0];
			if (ch == ';') // ignore comment lines
				continue;
			if (ch == '{') // increase depth level on opened bracket
			{ 
				if (++currentdepth == targetdepth)
					return; // return if file current depth level is equal to target depth level
			}
			else if (ch == '}')  // increase depth level on closed bracket but keeps reading if depth level is back to 0
			{ 
				if (--currentdepth < 0 || currentdepth == targetdepth)
					return; // return if file current depth level is equal to target depth level
			}
		}
	}

} // namespace rpp