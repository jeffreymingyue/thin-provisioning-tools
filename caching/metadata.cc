#include "caching/metadata.h"
#include "caching/superblock.h"
#include "persistent-data/space-maps/core.h"

using namespace caching;

//----------------------------------------------------------------

namespace {
	unsigned const METADATA_CACHE_SIZE = 1024;

	// FIXME: duplication
	transaction_manager::ptr
	open_tm(block_manager<>::ptr bm) {
		space_map::ptr sm(new core_map(bm->get_nr_blocks()));
		sm->inc(SUPERBLOCK_LOCATION);
		transaction_manager::ptr tm(new transaction_manager(bm, sm));
		return tm;
	}

	void
	copy_space_maps(space_map::ptr lhs, space_map::ptr rhs) {
		for (block_address b = 0; b < rhs->get_nr_blocks(); b++) {
			uint32_t count = rhs->get_count(b);
			if (count > 0)
				lhs->set_count(b, rhs->get_count(b));
		}
	}
}

//----------------------------------------------------------------

metadata::metadata(block_manager<>::ptr bm, open_type ot)
{
	switch (ot) {
	case CREATE:
		create_metadata(bm);
		break;

	case OPEN:
		open_metadata(bm);
		break;

	default:
		throw runtime_error("unhandled open_type");
	}
}

void
metadata::commit()
{
	commit_space_map();
	commit_mappings();
	commit_hints();
	commit_superblock();
}

void
metadata::setup_hint_array(size_t width)
{
	if (width > 0)
		hints_ = hint_array::ptr(
			new hint_array(tm_, width));
}

void
metadata::init_superblock()
{
#if 0
	sb_.magic_ = SUPERBLOCK_MAGIC;
	sb_.version_ = 1;
	sb_.data_mapping_root_ = mappings_->get_root();
	sb_.device_details_root_ = details_->get_root();
	sb_.data_block_size_ = data_block_size;
	sb_.metadata_block_size_ = MD_BLOCK_SIZE;
	sb_.metadata_nr_blocks_ = tm_->get_bm()->get_nr_blocks();
#endif
}

void
metadata::create_metadata(block_manager<>::ptr bm)
{
	tm_ = open_tm(bm);

	::memset(&sb_, 0, sizeof(sb_));
	init_superblock();

	space_map::ptr core = tm_->get_sm();
	metadata_sm_ = create_metadata_sm(tm_, tm_->get_bm()->get_nr_blocks());
	copy_space_maps(metadata_sm_, core);
	tm_->set_sm(metadata_sm_);

	mappings_ = mapping_array::ptr(new mapping_array(tm_, mapping_array::ref_counter()));

	// We can't instantiate the hint array yet, since we don't know the
	// hint width.
}

void
metadata::open_metadata(block_manager<>::ptr bm)
{
	tm_ = open_tm(bm);
	sb_ = read_superblock(tm_->get_bm());

	mappings_ = mapping_array::ptr(
		new mapping_array(tm_,
				  mapping_array::ref_counter(),
				  sb_.mapping_root,
				  sb_.cache_blocks));
}

void
metadata::commit_space_map()
{
	metadata_sm_->commit();
	metadata_sm_->copy_root(&sb_.metadata_space_map_root, sizeof(sb_.metadata_space_map_root));
}

void
metadata::commit_mappings()
{
	sb_.mapping_root = mappings_->get_root();
}

void
metadata::commit_hints()
{
	sb_.hint_root = hints_->get_root();
}

void
metadata::commit_superblock()
{
	write_superblock(tm_->get_bm(), sb_);
}

//----------------------------------------------------------------
