/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2021  gatecat <gatecat@ds0.me>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "nextpnr.h"

NEXTPNR_NAMESPACE_BEGIN

template <uint8_t bits> struct PackedArray {
	using Tchunk = uint64_t;
	using Tvalue = uint8_t;
	static constexpr Tchunk vals_per_chunk = (64 / bits);
	static constexpr Tchunk mask = ((1 << bits) - 1);

	PackedArray() = default;
	PackedArray(size_t size) { resize(size); };
	Tvalue get(size_t index) const {
		Tchunk ch = chunks.at(index / vals_per_chunk);
		return (ch >> ((index % vals_per_chunk) * bits)) & mask;
	}
	void set(size_t index, Tvalue value) {
		Tchunk &ch = chunks.at(index / vals_per_chunk);
		size_t bit_offset = (index % vals_per_chunk) * bits;
		ch &= ~(mask << (bit_offset));
		ch |= ((value & mask) << bit_offset);
	}
	void resize(size_t new_size) {
		chunks.resize((new_size + vals_per_chunk - 1) / vals_per_chunk);
	}
	std::vector<Tchunk> chunks;
};

NEXTPNR_NAMESPACE_END