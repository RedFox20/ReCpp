#include "binary_readwrite.h"

namespace rpp
{
	////////////////////////////////////////////////////////////////////////////

	binary_writer::binary_writer() : Pos(0), Sock(nullptr){}
	binary_writer::binary_writer(rpp::socket & s) : Pos(0), Sock(&s) {}
	binary_writer::~binary_writer() { if (Sock && Pos) Sock->send(Buf, Pos); }

	void binary_writer::flush() {
		Sock->send(Buf, Pos), Pos = 0;
	}
	binary_writer & binary_writer::write(const void * data, uint numBytes) {
		if (uint(MAX - Pos) < numBytes)
			flush(); // forced flush
		memcpy(&Buf[Pos], data, numBytes), Pos += numBytes;
		return *this;
	}

	////////////////////////////////////////////////////////////////////////////

	binary_reader::binary_reader() : Pos(0), Rem(0), Sock(nullptr) {}
	binary_reader::binary_reader(rpp::socket & s) : Pos(0), Rem(0), Sock(&s) {}

	void binary_reader::flush() {
		Pos = 0;
		if (Sock->available()) Sock->flush();
	}

	void binary_reader::buf_fill() {
		int n = Sock->recv(Buf, MAX);
		Pos = 0;
		Rem = (n > 0) ? n : 0;
	}

	uint binary_reader::buf_read(void * dst, uint cnt) {
		uint n = min<uint>(cnt, Rem);
		memcpy(dst, &Buf[Pos], cnt);
		Pos += n, Rem -= n;
		return n;
	}

	uint binary_reader::read(void * dst, uint cnt) {
		uint n = Rem;
		if (n >= cnt) return buf_read(dst, cnt); // best case, all from buffer
		return _read(&dst, cnt, n);                  // fallback partial read
	}

	uint binary_reader::_read(void * dst, uint cnt, uint bufn) {
		uint rem = cnt - buf_read(dst, bufn); // read some from buffer; won't read all(!)
		buf_fill();
		return bufn + buf_read((char*)dst + bufn, rem); // refill and read from buf
	}

	uint binary_reader::peek(void * dst, uint cnt) {
		if (Rem <= 0) buf_fill();  // fill before peek if possible
		memcpy(dst, &Buf[Pos], cnt);
		return cnt;
	}

	void binary_reader::skip(uint n) {
		uint nskip = min<uint>(n, Rem); // max skippable: Rem
		Pos += nskip, Rem -= nskip;
		if (nskip < n) Sock->skip(n - nskip); // skip remaining from storage
	}

	void binary_reader::undo(uint n) {
		uint nundo = min<uint>(n, Pos); // max undoable:  Pos
		Pos -= nundo, Rem += nundo;
	}
	////////////////////////////////////////////////////////////////////////////
}