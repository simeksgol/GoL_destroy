#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "lib.c"
#include "rect.c"
#include "randomarray.c"
#include "golgrid.c"
#include "celllist.c"
#include "gridmisc.c"
#include "hashtable.c"
#include "store.c"

#define GG_ARRAY_CNT 48
#define GRID_SIZE 256
#define MAX_PATTERN_SIZE (GRID_SIZE - 8)
#define MAX_FILENAME_SIZE 256
#define MAX_FILE_SIZE 65536
#define OBJECT_TYPE_CNT 13
#define MAX_POSS_OBJECTS 262144
#define MAX_MAX_OBJECTS 256
#define MAX_BYTE_SEQ_SIZE (3 + 3 * MAX_MAX_OBJECTS)
#define MAX_CENSUS_OBJECTS 512
#define MAX_GENS 32768
#define MAX_NEW_GENS 1024
#define COST_OFF 16384


typedef struct
{
	int object_type;
	int left_x;
	int top_y;
} AddedObject;

typedef struct
{
	int tree_id;
	double mid_x;
	double mid_y;
} CensusObject;

typedef struct
{
	int obj_1;
	int obj_2;
	double cost;
} EdgeCost;


static GoLGrid _gg [GG_ARRAY_CNT];
static GoLGrid *gg [GG_ARRAY_CNT];

static s32 poss_object_cnt = 0;
static AddedObject poss_object [MAX_POSS_OBJECTS];

static CensusObject census_obj [MAX_CENSUS_OBJECTS];
static EdgeCost edge_cost [(MAX_CENSUS_OBJECTS - 1) * MAX_CENSUS_OBJECTS / 2];

static const CellList_s8 *get_object_cell_list (int object_type)
{
	if (object_type == 0)
		return CellList_s8_get_block_cells ();
	if (object_type == 1)
		return CellList_s8_get_hive_cells (0);
	if (object_type == 2)
		return CellList_s8_get_hive_cells (1);
	if (object_type == 3)
		return CellList_s8_get_blinker_cells (0);
	if (object_type == 4)
		return CellList_s8_get_blinker_cells (1);
	if (object_type == 5)
		return CellList_s8_get_loaf_cells (0);
	if (object_type == 6)
		return CellList_s8_get_loaf_cells (1);
	if (object_type == 7)
		return CellList_s8_get_loaf_cells (2);
	if (object_type == 8)
		return CellList_s8_get_loaf_cells (3);
	if (object_type == 9)
		return CellList_s8_get_boat_cells (0);
	if (object_type == 10)
		return CellList_s8_get_boat_cells (1);
	if (object_type == 11)
		return CellList_s8_get_boat_cells (2);
	if (object_type == 12)
		return CellList_s8_get_boat_cells (3);
	
	return NULL;
}

static void store_object_list (const AddedObject *obj_list, int obj_cnt, s32 cost, ByteSeqStore *bss)
{
	int byte_seq_ix = 0;
	u8 byte_seq [MAX_BYTE_SEQ_SIZE];
	
	byte_seq [byte_seq_ix++] = (u8) (obj_cnt);
	
	int obj_ix;
	for (obj_ix = 0; obj_ix < obj_cnt; obj_ix++)
	{
		byte_seq [byte_seq_ix++] = (u8) (obj_list [obj_ix].object_type);
		byte_seq [byte_seq_ix++] = (u8) (obj_list [obj_ix].left_x);
		byte_seq [byte_seq_ix++] = (u8) (obj_list [obj_ix].top_y);
	}
	
	byte_seq [byte_seq_ix++] = (u8) (((u32) cost) >> 8);
	byte_seq [byte_seq_ix++] = (u8) (((u32) cost) & 0xff);
	
	ByteSeqStore_store (bss, byte_seq, byte_seq_ix);
}

static int get_object_list (const u8 *byte_seq, AddedObject *obj_list)
{
	s32 byte_seq_ix = 0;
	int obj_cnt = byte_seq [byte_seq_ix++];
	
	int obj_ix;
	for (obj_ix = 0; obj_ix < obj_cnt; obj_ix++)
	{
		obj_list [obj_ix].object_type = (int) byte_seq [byte_seq_ix++];
		obj_list [obj_ix].left_x = (int) (s8) byte_seq [byte_seq_ix++];
		obj_list [obj_ix].top_y = (int) (s8) byte_seq [byte_seq_ix++];
	}
	
	return obj_cnt;
}

static s32 census_pattern (const GoLGrid *pattern, CensusObject *obj, int max_obj)
{
	GoLGrid *remaining = gg [0];
	GoLGrid *cur_obj = gg [1];
	GoLGrid *bleed_temp = gg [2];
	GoLGrid *obj_bleed = gg [3];
	GoLGrid *new_obj = gg [4];
	
	GoLGrid_copy (pattern, remaining);
	
	int obj_ix = 0;
	s32 cell_x;
	s32 cell_y;
	
	while (TRUE)
	{
		if (!GoLGrid_find_next_on_cell_noinline (remaining, (obj_ix == 0), &cell_x, &cell_y))
			break;
		
		GoLGrid_clear_noinline (cur_obj);
		GoLGrid_set_cell_on (cur_obj, cell_x, cell_y);
		
		while (TRUE)
		{
			GoLGrid_bleed_4_noinline (cur_obj, bleed_temp);
			GoLGrid_bleed_8_noinline (bleed_temp, obj_bleed);
			
			GoLGrid_and_noinline (obj_bleed, remaining, new_obj);
			if (GoLGrid_is_equal_noinline (new_obj, cur_obj))
				break;
			
			GoLGrid_copy_noinline (new_obj, cur_obj);
		}
		
		if (obj_ix >= max_obj)
		{
			fprintf (stderr, "Too many objects in pattern census\n");
			exit (0);
		}
		
		Rect bb;
		GoLGrid_get_bounding_box (new_obj, &bb);
		
		obj [obj_ix].tree_id = obj_ix;
		obj [obj_ix].mid_x = ((double) bb.left_x) + (((double) bb.width) / 2.0);
		obj [obj_ix].mid_y = ((double) bb.top_y) + (((double) bb.height) / 2.0);
		obj_ix++;
		
		GoLGrid_subtract_noinline (remaining, new_obj);
	}
	
	return obj_ix;
}

static s32 get_cost (const u8 *byte_seq)
{
	s32 byte_seq_ix = 0;
	int obj_cnt = byte_seq [byte_seq_ix++];
	byte_seq_ix += (3 * obj_cnt);
	
	return (s32) ((((u32) (byte_seq [byte_seq_ix])) << 8) + (u32) (byte_seq [byte_seq_ix + 1]));
}

static double calc_edge_cost (const CensusObject *obj_1, const CensusObject *obj_2)
{
	double x_dist_sq = (obj_2->mid_x - obj_1->mid_x) * (obj_2->mid_x - obj_1->mid_x);
	double y_dist_sq = (obj_2->mid_y - obj_1->mid_y) * (obj_2->mid_y - obj_1->mid_y);
	double dist = sqrt (x_dist_sq + y_dist_sq);
	
	// Return (dist ^ 1.25), calling sqrt three times is much faster than a single call to pow
	return dist * sqrt (sqrt (dist));
}

static int compare_edges (const void *edge_1, const void *edge_2)
{
	if (((const EdgeCost *) edge_1)->cost < ((const EdgeCost *) edge_2)->cost)
		return -1;
	else if (((const EdgeCost *) edge_1)->cost > ((const EdgeCost *) edge_2)->cost)
		return 1;
	else
		return 0;
}

static s32 calc_cost (const GoLGrid *pattern)
{
	int census_cnt = census_pattern (pattern, census_obj, MAX_CENSUS_OBJECTS);
	
	s32 edge_ix = 0;
	int obj_1_ix;
	int obj_2_ix;
	for (obj_1_ix = 0; obj_1_ix < census_cnt; obj_1_ix++)
		for (obj_2_ix = obj_1_ix + 1; obj_2_ix < census_cnt; obj_2_ix++)
		{
			edge_cost [edge_ix].obj_1 = obj_1_ix;
			edge_cost [edge_ix].obj_2 = obj_2_ix;
			edge_cost [edge_ix].cost = calc_edge_cost (&census_obj [obj_1_ix], &census_obj [obj_2_ix]);
			edge_ix++;
		}
	
	s32 edge_cnt = edge_ix;
	qsort (edge_cost, edge_cnt, sizeof (edge_cost [0]), &compare_edges);
	
	double spanning_tree_size = 0.0;
	edge_ix = 0;
	int tree_cnt = census_cnt;
	
	while (tree_cnt > 1)
	{
		int tree_1 = census_obj [edge_cost [edge_ix].obj_1].tree_id;
		int tree_2 = census_obj [edge_cost [edge_ix].obj_2].tree_id;
		if (tree_1 != tree_2)
		{
			int obj_ix;
			for (obj_ix = 0; obj_ix < census_cnt; obj_ix++)
				if (census_obj [obj_ix].tree_id == tree_2)
					census_obj [obj_ix].tree_id = tree_1;
			
			spanning_tree_size += edge_cost [edge_ix].cost;
			tree_cnt--;
		}
		
		edge_ix++;
	}
	
	return lower_of_s32 (1 + (s32) (2.5 * spanning_tree_size), COST_OFF - 1);
}

static void object_list_to_grid (const AddedObject *obj_list, int obj_cnt, GoLGrid *out_gg)
{
	GoLGrid_clear (out_gg);
	
	int obj_ix;
	for (obj_ix = 0; obj_ix < obj_cnt; obj_ix++)
		GoLGrid_or_cell_list (out_gg, get_object_cell_list (obj_list [obj_ix].object_type), obj_list [obj_ix].left_x, obj_list [obj_ix].top_y);
}

static void byte_seq_to_grid (const u8 *byte_seq, GoLGrid *out_gg)
{
	GoLGrid_clear_noinline (out_gg);
	
	int obj_ix;
	for (obj_ix = 0; obj_ix < (s32) byte_seq [0]; obj_ix++)
		GoLGrid_or_cell_list (out_gg, get_object_cell_list ((int) byte_seq [1 + 3 * obj_ix]), (int) (s8) byte_seq [2 + 3 * obj_ix], (int) (s8) byte_seq [3 + 3 * obj_ix]);
}

static int run_setup (const GoLGrid *setup, const GoLGrid *allowed_area, const AddedObject *obj_list, int obj_cnt, ByteSeqStore *bss)
{
	GoLGrid *ev_m2 = gg [5];
	GoLGrid *ev_m1 = gg [6];
	GoLGrid *ev_p0 = gg [7];
	
	GoLGrid_copy_noinline (setup, ev_p0);
	
	s32 gen = 0;
	while (TRUE)
	{
		if (!GoLGrid_is_subset (ev_p0, allowed_area))
			return FALSE;
		
		if (gen >= 2 && GoLGrid_is_equal (ev_p0, ev_m2))
			break;
		
		if (gen >= MAX_NEW_GENS)
			return FALSE;
		
		GoLGrid *temp = ev_m2;
		ev_m2 = ev_m1;
		ev_m1 = ev_p0;
		ev_p0 = temp;
		
		GoLGrid_evolve (ev_m1, ev_p0);
		gen++;
	}
	
	if (GoLGrid_is_empty (ev_p0))
		return TRUE;
	
	store_object_list (obj_list, obj_cnt, calc_cost (ev_p0), bss);
	return FALSE;
}

static s32 gens_until_stable (const GoLGrid *pattern)
{
	GoLGrid *ev_m2 = gg [8];
	GoLGrid *ev_m1 = gg [9];
	GoLGrid *ev_p0 = gg [10];
	
	GoLGrid_copy_noinline (pattern, ev_p0);
	s32 gen = 0;
	while (TRUE)
	{
		if ((gen >= 2 && GoLGrid_is_equal_noinline (ev_p0, ev_m2)) || gen >= MAX_GENS)
			break;
		
		GoLGrid *temp = ev_m2;
		ev_m2 = ev_m1;
		ev_m1 = ev_p0;
		ev_p0 = temp;
		
		GoLGrid_evolve_noinline (ev_m1, ev_p0);
		gen++;
	}
	
	return gen;
}

static void run_for_gens (GoLGrid *pattern, s32 gens)
{
	GoLGrid *ev_m2 = gg [11];
	GoLGrid *ev_m1 = gg [12];
	GoLGrid *ev_p0 = gg [13];
	
	GoLGrid_copy_noinline (pattern, ev_p0);
	s32 gen;
	for (gen = 0; gen < gens; gen++)
	{
		GoLGrid *temp = ev_m2;
		ev_m2 = ev_m1;
		ev_m1 = ev_p0;
		ev_p0 = temp;
		
		GoLGrid_evolve_noinline (ev_m1, ev_p0);
	}
	
	GoLGrid_copy_noinline (ev_p0, pattern);
}

static void make_early_and_late_grids (const GoLGrid *pattern, s32 last_early_gen, s32 end_gen, GoLGrid *early_pattern, GoLGrid *early_envelope, GoLGrid *late_envelope)
{
	GoLGrid *ev_m2 = gg [14];
	GoLGrid *ev_m1 = gg [15];
	GoLGrid *ev_p0 = gg [16];
	
	// Default to starting pattern
	GoLGrid_copy_noinline (pattern, early_pattern);
	
	// Default to empty grid
	GoLGrid_clear_noinline (early_envelope);
	
	GoLGrid_copy_noinline (pattern, ev_p0);
	GoLGrid_clear_noinline (late_envelope);
	
	s32 gen = 0;
	while (TRUE)
	{
		GoLGrid_or_noinline (late_envelope, ev_p0);
		if (gen == last_early_gen)
		{
			GoLGrid_copy_noinline (ev_p0, early_pattern);
			GoLGrid_copy_noinline (late_envelope, early_envelope);
			GoLGrid_clear_noinline (late_envelope);
		}
		
		if (gen == end_gen)
			break;
		
		GoLGrid *temp = ev_m2;
		ev_m2 = ev_m1;
		ev_m1 = ev_p0;
		ev_p0 = temp;
		
		GoLGrid_evolve_noinline (ev_m1, ev_p0);
		gen++;
	}
}

static s32 cost_from_scratch (const AddedObject *obj_list, int obj_cnt, const GoLGrid *problem)
{
	GoLGrid *in_setup = gg [17];
	GoLGrid *current_objects = gg [18];
	
	GoLGrid_copy_noinline (problem, in_setup);
	object_list_to_grid (obj_list, obj_cnt, current_objects);
	GoLGrid_or_noinline (in_setup, current_objects);
	
	s32 stable_gen = gens_until_stable (in_setup);
	run_for_gens (in_setup, stable_gen);
	
	return calc_cost (in_setup);
}

static int add_next_object (AddedObject *obj_list, int in_obj_cnt, const GoLGrid *problem, const GoLGrid *cat_area, const GoLGrid *allowed_area, s32 late_phase_gens,
		HashTable_u64 *seen_starting_points, HashTable_u64 *tested_setups, const RandomDataArray *rda, ByteSeqStore *out_bss)
{
	GoLGrid *in_setup = gg [19];
	GoLGrid *current_objects = gg [20];
	GoLGrid *starting_point = gg [21];
	GoLGrid *early_envelope = gg [22];
	GoLGrid *late_envelope = gg [23];
	GoLGrid *forbidden_area = gg [24];
	GoLGrid *useable_cat_area = gg [25];
	GoLGrid *area_temp = gg [26];
	GoLGrid *must_touch_area = gg [27];
	GoLGrid *current_objects_p2 = gg [28];
	GoLGrid *locked_out_area = gg [29];
	GoLGrid *new_object = gg [30];
	GoLGrid *new_object_p2 = gg [31];
	GoLGrid *all_objects = gg [32];
	GoLGrid *setup = gg [33];
	GoLGrid *result_gg = gg [34];
	
	GoLGrid_copy_noinline (problem, in_setup);
	object_list_to_grid (obj_list, in_obj_cnt, current_objects);
	GoLGrid_or_noinline (in_setup, current_objects);
	
	s32 stable_gen = gens_until_stable (in_setup);
	
	// Needs to be an even number, to add the new object in the right phase
	s32 last_early_gen = (in_obj_cnt == 0 ? -1 : higher_of_s32 (-1, align_down_s32 (stable_gen - late_phase_gens, 2)));
	
	make_early_and_late_grids (in_setup, last_early_gen, stable_gen, starting_point, early_envelope, late_envelope);
	
	GoLGrid_bleed_8_noinline (early_envelope, area_temp);
	GoLGrid_bleed_4_noinline (area_temp, forbidden_area);
	
	GoLGrid_bleed_8_noinline (late_envelope, area_temp);
	GoLGrid_bleed_4_noinline (area_temp, must_touch_area);
	
	GoLGrid_and_noinline (cat_area, must_touch_area, useable_cat_area);
	GoLGrid_subtract_noinline (useable_cat_area, forbidden_area);
	
	u64 seen_hash = GoLGrid_get_hash_noinline (starting_point, rda) ^ GoLGrid_get_hash_noinline (useable_cat_area, rda);
	int was_present;
	HashTable_u64_store (seen_starting_points, seen_hash, 0, FALSE, &was_present);
	if (was_present)
		return FALSE;
	
	GoLGrid_evolve_noinline (current_objects, current_objects_p2);
	GoLGrid_or_noinline (current_objects_p2, current_objects);
	
	GoLGrid_bleed_8_noinline (current_objects_p2, area_temp);
	GoLGrid_bleed_4_noinline (area_temp, locked_out_area);
	
	s32 new_object_ix;
	for (new_object_ix = 0; new_object_ix < poss_object_cnt; new_object_ix++)
	{
		obj_list [in_obj_cnt].object_type = poss_object [new_object_ix].object_type;
		obj_list [in_obj_cnt].left_x = poss_object [new_object_ix].left_x;
		obj_list [in_obj_cnt].top_y = poss_object [new_object_ix].top_y;
		
		object_list_to_grid (&obj_list [in_obj_cnt], 1, new_object);
		GoLGrid_evolve_noinline (new_object, new_object_p2);
		GoLGrid_or_noinline (new_object_p2, new_object);
		
		if (!(GoLGrid_are_disjoint_noinline (new_object_p2, forbidden_area)))
			continue;
		
		if (GoLGrid_are_disjoint_noinline (new_object_p2, must_touch_area))
			continue;
		
		if (!(GoLGrid_are_disjoint_noinline (new_object_p2, locked_out_area)))
			continue;
		
		GoLGrid_copy_noinline (new_object, all_objects);
		GoLGrid_or_noinline (all_objects, current_objects);
		
		u64 objects_hash = GoLGrid_get_hash_noinline (all_objects, rda);
		HashTable_u64_store (tested_setups, objects_hash, 0, FALSE, &was_present);
		if (was_present)
			continue;
		
		GoLGrid_copy_noinline (starting_point, setup);
		GoLGrid_or_noinline (setup, new_object);
		
		if (run_setup (setup, allowed_area, obj_list, in_obj_cnt + 1, out_bss))
		{
			printf ("Found a solution:\n\n");
			
			GoLGrid_copy_noinline (in_setup, result_gg);
			GoLGrid_or_noinline (result_gg, new_object);
			
			GoLGrid_print_life_history_full (NULL, NULL, result_gg, all_objects, NULL, NULL);
			printf ("\n");
			
			int obj_ix;
			for (obj_ix = 1; obj_ix < in_obj_cnt + 1; obj_ix++)
				printf ("Cost with first %2d objects: %4d\n", obj_ix, cost_from_scratch (obj_list, obj_ix, problem));
			
			return TRUE;
		}
	}
	
	return FALSE;
}

static void count_cost_slots (const ByteSeqStore *bss, s32 cost_cnt [])
{
	ByteSeqStoreNode *bss_node;
	s32 node_data_offset;
	ByteSeqStore_start_get_iteration (bss, &bss_node, &node_data_offset);
	
	while (TRUE)
	{
		u8 byte_seq [MAX_BYTE_SEQ_SIZE];
		if (!ByteSeqStore_get_next (bss, &bss_node, &node_data_offset, byte_seq, MAX_BYTE_SEQ_SIZE, NULL))
			break;
		
		cost_cnt [get_cost (byte_seq)]++;
	}
}

static void filter_bss (const ByteSeqStore *in_bss, ByteSeqStore *out_bss, s32 cost_limit, s32 patterns_on_cost_limit, s32 use_on_cost_limit)
{
	ByteSeqStoreNode *bss_node;
	s32 node_data_offset;
	ByteSeqStore_start_get_iteration (in_bss, &bss_node, &node_data_offset);
	
	while (TRUE)
	{
		u8 byte_seq [MAX_BYTE_SEQ_SIZE];
		s32 seq_size;
		if (!ByteSeqStore_get_next (in_bss, &bss_node, &node_data_offset, byte_seq, MAX_BYTE_SEQ_SIZE, &seq_size))
			break;
		
		s32 cost = get_cost (byte_seq);
		if (cost > cost_limit)
			continue;
		
		if (cost == cost_limit)
		{
			double prob = (double) use_on_cost_limit / (double) patterns_on_cost_limit;
			double rand = (double) random_u64 () / 18446744073709551616.0;
			
			patterns_on_cost_limit--;
			if (rand >= prob)
				continue;
			
			use_on_cost_limit--;
		}
		
		ByteSeqStore_store (out_bss, byte_seq, seq_size);
	}
}

static void print_lowest_cost (const ByteSeqStore *bss, const GoLGrid *problem)
{
	GoLGrid *objects_gg = gg [35];
	GoLGrid *show_gg = gg [36];
	
	ByteSeqStoreNode *bss_node;
	s32 node_data_offset;
	u8 byte_seq [MAX_BYTE_SEQ_SIZE];
	ByteSeqStore_start_get_iteration (bss, &bss_node, &node_data_offset);
	
	s32 min_cost = COST_OFF;
	while (TRUE)
	{
		if (!ByteSeqStore_get_next (bss, &bss_node, &node_data_offset, byte_seq, MAX_BYTE_SEQ_SIZE, NULL))
			break;
		
		min_cost = lower_of_s32 (min_cost, get_cost (byte_seq));
	}
	
	ByteSeqStore_start_get_iteration (bss, &bss_node, &node_data_offset);
	while (TRUE)
	{
		if (!ByteSeqStore_get_next (bss, &bss_node, &node_data_offset, byte_seq, MAX_BYTE_SEQ_SIZE, NULL))
			break;
		
		if (get_cost (byte_seq) == min_cost)
		{
			GoLGrid_copy_noinline (problem, show_gg);
			byte_seq_to_grid (byte_seq, objects_gg);
			GoLGrid_or_noinline (show_gg, objects_gg);
			
			fprintf (stderr, "Lowest cost (%d) intermediate:\n\n", min_cost);
			GoLGrid_print_life_history_full (stderr, NULL, show_gg, objects_gg, NULL, NULL);
			fprintf (stderr, "\n");
			
			break;
		}
	}
}

static void make_poss_objects (const GoLGrid *cat_area, int *use_object_type)
{
	GoLGrid *object_gg = gg [37];
	GoLGrid *object_p2 = gg [38];
	
	int y_ix;
	int x_ix;
	int obj_ix;
	
	for (obj_ix = 0; obj_ix < OBJECT_TYPE_CNT; obj_ix++)
		if (use_object_type [obj_ix])
			for (y_ix = -(MAX_PATTERN_SIZE / 2); y_ix < MAX_PATTERN_SIZE / 2; y_ix++)
				for (x_ix = -(MAX_PATTERN_SIZE / 2); x_ix < MAX_PATTERN_SIZE / 2; x_ix++)
				{
					GoLGrid_clear_noinline (object_gg);
					GoLGrid_or_cell_list (object_gg, get_object_cell_list (obj_ix), x_ix, y_ix);
					GoLGrid_evolve_noinline (object_gg, object_p2);
					GoLGrid_or_noinline (object_p2, object_gg);
					
					if (GoLGrid_is_subset_noinline (object_p2, cat_area))
					{
						if (poss_object_cnt >= MAX_POSS_OBJECTS)
						{
							fprintf (stderr, "Overflow in poss_object list\n");
							exit (0);
						}
						
						poss_object [poss_object_cnt].object_type = obj_ix;
						poss_object [poss_object_cnt].left_x = x_ix;
						poss_object [poss_object_cnt].top_y = y_ix;
						poss_object_cnt++;
					}
				}
}

static int parse_spec_file (const char *filename, GoLGrid *problem, GoLGrid *cat_area, GoLGrid *allowed_area)
{
	char name_buf [MAX_FILENAME_SIZE + 16];
	char file_buf [MAX_FILE_SIZE + 1];
	
	if (strlen (filename) > MAX_FILENAME_SIZE)
	{
		fprintf (stderr, "Filename too long\n");
		return FALSE;
	}
	
	strcpy (name_buf, filename);
	strcpy (name_buf + strlen (name_buf), ".rle");
	
	FILE *spec_file = fopen (name_buf, "r");
	if (spec_file == NULL)
	{
		strcpy (name_buf, filename);
		spec_file = fopen (name_buf, "r");
		if (spec_file == NULL)
		{
			fprintf (stderr, "Failed to open pattern file\n");
			return FALSE;
		}
		
	}
	
	s32 file_size = fread (file_buf, 1, MAX_FILE_SIZE, spec_file);
	fclose (spec_file);
	
	if (file_size >= MAX_FILE_SIZE)
	{
		fprintf (stderr, "Pattern file size to large\n");
		return FALSE;
	}
	
	file_buf [file_size] = '\0';
	
	s32 start_ix = 0;
	int skip = FALSE;
	while (TRUE)
	{
		char c = file_buf [start_ix];
		
		if (c == '\0')
			break;
		else if (c == '#' || c == 'x')
			skip = TRUE;
		else if (c == '\n' || c == '\r')
			skip = FALSE;
		else if (!skip)
			break;
		
		start_ix++;
	}
	
	int clipped;
	if (!GoLGrid_parse_life_history (&file_buf [start_ix], -(MAX_PATTERN_SIZE / 2), -(MAX_PATTERN_SIZE / 2), problem, cat_area, allowed_area, NULL, &clipped, NULL))
	{
		fprintf (stderr, "Illegal pattern file\n");
		return FALSE;
	}
	
	GoLGrid_or_noinline (allowed_area, problem);
	GoLGrid_or_noinline (allowed_area, cat_area);
	
	Rect pr;
	GoLGrid_get_bounding_box (allowed_area, &pr);
	
	if (clipped || pr.width > MAX_PATTERN_SIZE || pr.height > MAX_PATTERN_SIZE)
	{
		fprintf (stderr, "Pattern too large, max is %d by %d cells\n", MAX_PATTERN_SIZE, MAX_PATTERN_SIZE);
		return FALSE;
	}
	
	return TRUE;
}

static int parse_object_type (const char *digits, int *use_object_type)
{
	int obj_ix;
	for (obj_ix = 0; obj_ix < OBJECT_TYPE_CNT; obj_ix++)
		use_object_type [obj_ix] = FALSE;
	
	int digits_ix = 0;
	while (TRUE)
	{
		char c = digits [digits_ix];
		
		if (c == '\0')
			return TRUE;
		else if (c == '1')
			use_object_type [0] = TRUE;
		else if (c == '2')
		{
			use_object_type [1] = TRUE;
			use_object_type [2] = TRUE;
		}
		else if (c == '3')
		{
			use_object_type [3] = TRUE;
			use_object_type [4] = TRUE;
		}
		else if (c == '4')
		{
			use_object_type [5] = TRUE;
			use_object_type [6] = TRUE;
			use_object_type [7] = TRUE;
			use_object_type [8] = TRUE;
		}
		else if (c == '5')
		{
			use_object_type [9] = TRUE;
			use_object_type [10] = TRUE;
			use_object_type [11] = TRUE;
			use_object_type [12] = TRUE;
		}
		else
		{
			fprintf (stderr, "Illegal object type \"%c\"\n", c);
			return FALSE;
		}
		
		digits_ix++;
	}
}	

static int has_active_part (const GoLGrid *pattern)
{
	GoLGrid *gen_1 = gg [39];
	GoLGrid *gen_2 = gg [40];
	
	GoLGrid_evolve_noinline (pattern, gen_1);
	GoLGrid_evolve_noinline (gen_1, gen_2);
	
	return (!(GoLGrid_is_equal (pattern, gen_2)));
}

static s32 preprocess_spec (const GoLGrid *problem, GoLGrid *cat_area)
{
	GoLGrid *temp_bleed = gg [41];
	GoLGrid *problem_bleed = gg [42];
	GoLGrid *removed_cat_area = gg [43];
	
	GoLGrid_subtract_noinline (cat_area, problem);
	
	GoLGrid_bleed_8_noinline (problem, temp_bleed);
	GoLGrid_bleed_4_noinline (temp_bleed, problem_bleed);
	
	GoLGrid_and_noinline (problem_bleed, cat_area, removed_cat_area);
	GoLGrid_subtract_noinline (cat_area, problem_bleed);
	
	return GoLGrid_get_population_noinline (removed_cat_area);
}

static int main_do (int argc, const char *const *argv)
{
	if (argc != 5)
	{
		fprintf (stderr, "USAGE:   destroy <pattern file> <objects> <max pool size> <max objects>\n");
		fprintf (stderr, "example: destroy demonoid.rle 124 5000 32\n");
		fprintf (stderr, "<objects> is a digit for each type of object to be used:\n");
		fprintf (stderr, "1 = block, 2 = hive, 3 = blinker, 4 = loaf, 5 = boat\n");
		return EXIT_FAILURE;
	}
	
	Rect gr;
	Rect_make (&gr, -(GRID_SIZE / 2), -(GRID_SIZE / 2), GRID_SIZE, GRID_SIZE);
	
	int gg_ix;
	for (gg_ix = 0; gg_ix < GG_ARRAY_CNT; gg_ix++)
	{
		GoLGrid_create (&_gg [gg_ix], &gr);
		gg [gg_ix] = &_gg [gg_ix];
	}
	
	GoLGrid *problem = gg [44];
	GoLGrid *cat_area = gg [45];
	GoLGrid *allowed_area = gg [46];
	
	if (!parse_spec_file (argv [1], problem, cat_area, allowed_area))
		return EXIT_FAILURE;
	
	int use_object_type [OBJECT_TYPE_CNT];
	
	if (!parse_object_type (argv [2], use_object_type))
		return EXIT_FAILURE;
	
	u32 parm_max_pool_size;
	u32 parm_max_objects;

	if (!str_to_u32 (argv [3], &parm_max_pool_size))
	{
		fprintf (stderr, "Illegal <max pool size> parameter\n");
		return EXIT_FAILURE;
	}
	
	if (!str_to_u32 (argv [4], &parm_max_objects))
	{
		fprintf (stderr, "Illegal <max objects> parameter\n");
		return EXIT_FAILURE;
	}
	if (parm_max_objects > MAX_MAX_OBJECTS)
	{
		fprintf (stderr, "Max value for <max objects> is %d\n", MAX_MAX_OBJECTS);
		return EXIT_FAILURE;
	}
	
	if (!has_active_part (problem))
	{
		fprintf (stderr, "Error: pattern has no active part to initiate the destruction\n");
		return EXIT_FAILURE;
	}
	
	s32 removed_cat_cells = preprocess_spec (problem, cat_area);
	
	fprintf (stderr, "Parsed pattern file:\n");
	GoLGrid_print_life_history_full (stderr, NULL, problem, cat_area, allowed_area, NULL);
	
	if (removed_cat_cells > 0)
		fprintf (stderr, "\nNote: %d cells with state 4 were too close to an on-cell and were changed\nto state 2\n", removed_cat_cells);
	
	s32 max_pool_size = (s32) parm_max_pool_size;
	s32 max_added_objects = (s32) parm_max_objects;
	s32 late_phase_gens = 256;
	
	make_poss_objects (cat_area, use_object_type);
	fprintf (stderr, "\nPossible objects in allowed area: %d\n\n", poss_object_cnt);
	
	RandomDataArray rda;
	RandomDataArray_create (&rda, gr.height * gr.width / 64);
	
	HashTable_u64 tested_setups;
	HashTable_u64_create (&tested_setups, 64, 0.7, 0.9);
	
	HashTable_u64 seen_starting_points;
	HashTable_u64_create (&seen_starting_points, 64, 0.7, 0.9);
	
	ByteSeqStore filtered;
	ByteSeqStore_create (&filtered, 16384);
	
	AddedObject obj_list [MAX_MAX_OBJECTS];
	store_object_list (obj_list, 0, 0, &filtered);
	
	ByteSeqStore unfiltered;
	ByteSeqStore_create (&unfiltered, 16384);
	
	int obj_cnt;
	for (obj_cnt = 1; obj_cnt <= max_added_objects; obj_cnt++)
	{
		if (obj_cnt > 1)
			print_lowest_cost (&filtered, problem);
		
		ByteSeqStore_clear (&unfiltered);
		
		ByteSeqStoreNode *bss_node;
		s32 node_data_offset;
		
		ByteSeqStore_start_get_iteration (&filtered, &bss_node, &node_data_offset);
		
		fprintf (stderr, "--- Starting round with %d added objects\n", obj_cnt);
		fprintf (stderr, "Filtered patterns = %d\n", (int) filtered.seq_count);
		
		HashTable_u64_clear (&seen_starting_points);
		
		s32 pattern_ix;
		for (pattern_ix = 0; pattern_ix < filtered.seq_count; pattern_ix++)
		{
			if (pattern_ix % 1000 == 0)
				fprintf (stderr, "Testing pattern %d\n", pattern_ix);
			
			u8 byte_seq [MAX_BYTE_SEQ_SIZE];
			ByteSeqStore_get_next (&filtered, &bss_node, &node_data_offset, byte_seq, MAX_BYTE_SEQ_SIZE, NULL);
			
			get_object_list (byte_seq, obj_list);
			if (add_next_object (obj_list, obj_cnt - 1, problem, cat_area, allowed_area, late_phase_gens, &seen_starting_points, &tested_setups, &rda, &unfiltered))
				return EXIT_SUCCESS;
		}
		
		if (unfiltered.seq_count == 0)
		{
			fprintf (stderr, "\nNo continuation found, ending search\n");
			break;
		}
		
		fprintf (stderr, "Unfiltered patterns: %d\n", (s32) unfiltered.seq_count);
		
		s32 cost_slot [COST_OFF];
		s32 cost_ix;
		for (cost_ix = 0; cost_ix < COST_OFF; cost_ix++)
			cost_slot [cost_ix] = 0;
		
		count_cost_slots (&unfiltered, cost_slot);
		
		s32 lowest_cost = 0;
		s32 old_pool_size = 0;
		s32 new_pool_size = 0;
		int reported = 0;
		for (cost_ix = 0; cost_ix < COST_OFF; cost_ix++)
		{
			if (cost_slot [cost_ix] != 0 && reported < 16)
			{
				if (reported == 0)
					lowest_cost = cost_ix;
				
				fprintf (stderr, "Patterns with cost %d: %d\n", cost_ix, cost_slot [cost_ix]);
				reported++;
			}
			
			old_pool_size = new_pool_size;
			new_pool_size += cost_slot [cost_ix];
			
			if (new_pool_size > max_pool_size)
				break;
		}
		
		printf ("%d objects, cost range: %d - %d\n", obj_cnt, lowest_cost, cost_ix);
		
		ByteSeqStore_clear (&filtered);
		filter_bss (&unfiltered, &filtered, cost_ix, new_pool_size - old_pool_size, max_pool_size - old_pool_size);
	}
	
	return EXIT_SUCCESS;
}

int main (int argc, const char *const *argv)
{
	if (!verify_cpu_type (FALSE, FALSE))
		return EXIT_FAILURE;
	
	if (!main_do (argc, argv))
		return EXIT_FAILURE;
	
	return EXIT_SUCCESS;
}
