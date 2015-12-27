#include "binary_serializer.h"

namespace rpp
{
	int binary_serializer::layout_size() const noexcept
	{
		int size = sizeof(length);
		auto p = (char*)this + sizeof(*this);

		for (auto v = layout; v > 0; v /= 10) switch (v % 10) {
			case 1: p += 1; size += 1; continue;
			case 2: p += 2; size += 2; continue;
			case 3: p += 4; size += 4; continue;
			case 4: p += 8; size += 8; continue;
			case 5: {
				const std::string& str = *(std::string*)p; // dangerous stuff...
				size += 2 + (int)str.size();
				p += sizeof(std::string);
				continue;
			}
			default:
				assert(!"AutoSerialize: Invalid layout specifier.");
				v = 0; // stop deserialization
		}
		return size;
	}
} // namespace rpp