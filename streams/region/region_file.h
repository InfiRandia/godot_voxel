#ifndef REGION_FILE_H
#define REGION_FILE_H

#include "../../storage/voxel_buffer.h"
#include "../../util/fixed_array.h"
#include "../../util/math/color8.h"
#include "../../util/math/vector3i.h"
#include <vector>

class FileAccess;
class VoxelBlockSerializerInternal;

struct VoxelRegionFormat {
	static const char *FILE_EXTENSION;
	static const uint32_t MAX_BLOCKS_ACROSS = 255;
	static const uint32_t CHANNEL_COUNT = 8;

	static_assert(CHANNEL_COUNT == VoxelBuffer::MAX_CHANNELS, "This format doesn't support variable channel count");

	// How many voxels in a cubic block, as power of two
	uint8_t block_size_po2 = 0;
	// How many blocks across all dimensions (stored as 3 bytes)
	Vector3i region_size;
	FixedArray<VoxelBuffer::Depth, CHANNEL_COUNT> channel_depths;
	// Blocks are stored at offsets multiple of that size
	uint32_t sector_size = 0;
	FixedArray<Color8, 256> palette;
	bool has_palette = false;

	bool validate() const;
	bool verify_block(const VoxelBuffer &block) const;
};

struct VoxelRegionBlockInfo {
	static const unsigned int MAX_SECTOR_INDEX = 0xffffff;
	static const unsigned int MAX_SECTOR_COUNT = 0xff;

	// AAAB
	// A: 3 bytes for sector index
	// B: 1 byte for size of the block, in sectors
	uint32_t data = 0;

	inline uint32_t get_sector_index() const {
		return data >> 8;
	}

	inline void set_sector_index(uint32_t i) {
		CRASH_COND(i > MAX_SECTOR_INDEX);
		data = (i << 8) | (data & 0xff);
	}

	inline uint32_t get_sector_count() const {
		return data & 0xff;
	}

	inline void set_sector_count(uint32_t c) {
		CRASH_COND(c > 0xff);
		data = (c & 0xff) | (data & 0xffffff00);
	}
};

// Archive file storing voxels in a fixed sparse grid data structure.
// The format is designed to be easily writable in chunks so it can be used for partial in-game loading and saving.
// Inspired by https://www.seedofandromeda.com/blogs/1-creating-a-region-file-system-for-a-voxel-game
// (if that link doesn't work, it can be found on Wayback Machine)
//
// This is a stream implementation, where the file handle remains in use for read and write and only keeps a fraction
// of data in memory.
// It isn't thread-safe.
//
class VoxelRegionFile {
public:
	VoxelRegionFile();
	~VoxelRegionFile();

	Error open(const String &fpath, bool create_if_not_found);
	Error close();
	bool is_open() const;

	bool set_format(const VoxelRegionFormat &format);
	const VoxelRegionFormat &get_format() const;

	Error load_block(Vector3i position, Ref<VoxelBuffer> out_block, VoxelBlockSerializerInternal &serializer);
	Error save_block(Vector3i position, Ref<VoxelBuffer> block, VoxelBlockSerializerInternal &serializer);

	unsigned int get_header_block_count() const;
	bool has_block(Vector3i position) const;
	bool has_block(unsigned int index) const;
	Vector3i get_block_position_from_index(uint32_t i) const;

	void debug_check();

private:
	bool save_header(FileAccess *f);
	Error load_header(FileAccess *f);

	unsigned int get_block_index_in_header(const Vector3i &rpos) const;
	uint32_t get_sector_count_from_bytes(uint32_t size_in_bytes) const;

	void pad_to_sector_size(FileAccess *f);
	void remove_sectors_from_block(Vector3i block_pos, unsigned int p_sector_count);

	bool migrate_to_latest(FileAccess *f);
	bool migrate_from_v2_to_v3(FileAccess *f, VoxelRegionFormat &format);

	struct Header {
		uint8_t version = -1;
		VoxelRegionFormat format;
		// Location and size of blocks, indexed by flat position.
		// This table always has the same size,
		// and the same index always corresponds to the same 3D position.
		std::vector<VoxelRegionBlockInfo> blocks;
	};

	FileAccess *_file_access = nullptr;
	bool _header_modified = false;

	Header _header;

	struct Vector3u16 {
		uint16_t x;
		uint16_t y;
		uint16_t z;

		Vector3u16(Vector3i p) :
				x(p.x), y(p.y), z(p.z) {}
	};

	// TODO Is it ever read?
	// List of sectors in the order they appear in the file,
	// and which position their block is. The same block can span multiple sectors.
	// This is essentially a reverse table of `Header::blocks`.
	std::vector<Vector3u16> _sectors;
	uint32_t _blocks_begin_offset;
	String _file_path;
};

#endif // REGION_FILE_H
