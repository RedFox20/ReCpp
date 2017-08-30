#include "binary_readwrite.h"

namespace rpp
{
	////////////////////////////////////////////////////////////////////////////

	socket_writer::socket_writer() : Pos(0), Sock(nullptr){}
	socket_writer::socket_writer(rpp::socket & s) : Pos(0), Sock(&s) {}
	socket_writer::~socket_writer() { if (Sock && Pos) Sock->send(Buf, Pos); }

	void socket_writer::flush() {
		Sock->send(Buf, Pos), Pos = 0;
	}
	socket_writer & socket_writer::write(const void * data, uint numBytes) {
		if (uint(MAX - Pos) < numBytes)
			flush(); // forced flush
		memcpy(&Buf[Pos], data, numBytes), Pos += numBytes;
		return *this;
	}

	////////////////////////////////////////////////////////////////////////////

	socket_reader::socket_reader() : Pos(0), Rem(0), Sock(nullptr) {}
	socket_reader::socket_reader(rpp::socket & s) : Pos(0), Rem(0), Sock(&s) {}

	void socket_reader::flush() {
		Pos = 0;
		if (Sock->available()) Sock->flush();
	}

	void socket_reader::buf_fill() {
		int n = Sock->recv(Buf, MAX);
		Pos = 0;
		Rem = (n > 0) ? n : 0;
	}

	uint socket_reader::buf_read(void * dst, uint cnt) {
		uint n = min<uint>(cnt, Rem);
		memcpy(dst, &Buf[Pos], cnt);
		Pos += n, Rem -= n;
		return n;
	}

	uint socket_reader::read(void * dst, uint cnt) {
		uint n = Rem;
		if (n >= cnt) return buf_read(dst, cnt); // best case, all from buffer
		return _read(&dst, cnt, n);                  // fallback partial read
	}

	uint socket_reader::_read(void * dst, uint cnt, uint bufn) {
		uint rem = cnt - buf_read(dst, bufn); // read some from buffer; won't read all(!)
		buf_fill();
		return bufn + buf_read((char*)dst + bufn, rem); // refill and read from buf
	}

	uint socket_reader::peek(void * dst, uint cnt) {
		if (Rem <= 0) buf_fill();  // fill before peek if possible
		memcpy(dst, &Buf[Pos], cnt);
		return cnt;
	}

	void socket_reader::skip(uint n) {
		uint nskip = min<uint>(n, Rem); // max skippable: Rem
		Pos += nskip, Rem -= nskip;
		if (nskip < n) Sock->skip(n - nskip); // skip remaining from storage
	}

	void socket_reader::undo(uint n) {
		uint nundo = min<uint>(n, Pos); // max undoable:  Pos
		Pos -= nundo, Rem += nundo;
	}
	////////////////////////////////////////////////////////////////////////////
}