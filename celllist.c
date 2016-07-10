// A description of a glider
// (dir) is 0 for a NW, 1 for a NE, 2 for a SE and 3 for SW-bound glider
// (lane) is the x-coordinate for the center cell of the glider if it is moved backwards or forwards in time, so that its center cell has y-coordinate 0
// and it is in the phase with three cells in a horizontal line
// (timing) is the generation if the glider is moved backwards or forwards in time, so that its center cell has x-coordinate 0 (instead of y-coordinate
// as for (lane)) and with the same phase as above

typedef struct
{
	int dir;
	s32 lane;
	s32 timing;
} Glider;

// A description of a standard spaceship
// (size) is 0 for glider, 1 for LWSS, 2 for MWSS and 3 for HWSS
// For a glider, (dir), (lane) and (timing) is defined as for the Glider type above, and (parity) is 0
// For a XWSS, (dir) is 0 for a N, 1 for an E, 2 for a S and 3 for a W-bound ship
// (lane) is the x- or y-coordinate of the symmetry axis of the different phases
// (timing) is the generation if the spaceship is moved backwards or forwards in time, so that it is in the phase with the low population count, and the off-cell with 5 on-cell neighbours has y-coordinate 0
// for a N or S-bound ship, or x-coordinate 0 for a W or E-bound ship
// (parity) is 0 if the "arrow" in the phase with the low population count points towards NW or SE when generation = (timing), and 1 if it points towards NE or SW at that time

// Suggested new definition of (parity) (not implemented):
// (parity) is 0 if, when the XWSS is in a phase with the low population count and where the sum of the x- och y-coordinate of the off-cell with 5 on-cell neighbours is an even number, the "arrow" points
// to the left when looking in the direction of travel of the XWSS, or 1 otherwise


typedef struct
{
	int size;
	int dir;
	s32 lane;
	s32 timing;
	s32 parity;
} StandardSpaceship;

typedef struct
{
	s8 x;
	s8 y;
} Coord_s8;

typedef struct
{
	s32 cell_count;
	s32 max_cells;
	Coord_s8 *cell;
} CellList_s8;

typedef struct
{
	s32 cell_list_ix;
	s32 coord_ix;
	GoLGrid temp_gg;
	GoLGrid zero_gg_1;
	GoLGrid zero_gg_2;
} CellList_s8_InitEnv;


#define CELLLIST_S8_STATIC_CELL_LIST_SIZE 80
#define CELLLIST_S8_STATIC_COORD_SIZE 1024

static int GoLGrid_to_cell_list (const GoLGrid *gg, CellList_s8 *cl);
static int GoLGrid_parse_life_history_simple (const char *lh, s32 left_x, s32 top_y, GoLGrid *on_gg);

static int CellList_s8_is_initialized = FALSE;

static Coord_s8 CellList_s8_static_coord [CELLLIST_S8_STATIC_COORD_SIZE];
static CellList_s8 CellList_s8_static_cell_list [CELLLIST_S8_STATIC_CELL_LIST_SIZE];

static const CellList_s8 *CellList_s8_block_cells;
static const CellList_s8 *CellList_s8_blinker_cells [2];
static const CellList_s8 *CellList_s8_hive_cells [2];
static const CellList_s8 *CellList_s8_loaf_cells [4];
static const CellList_s8 *CellList_s8_boat_cells [4];
static const CellList_s8 *CellList_s8_pond_cells;

static const CellList_s8 *CellList_s8_spaceship_cells [4] [4] [4];
static const char *CellList_s8_spaceship_spec [4] = {"3o$o$bo!", "3o$o2bo$o$o$bobo!", "3o$o2bo$o$o3bo$o$bobo!", "3o$o2bo$o$o3bo$o3bo$o$bobo!"};
static const int CellList_s8_spaceship_orientation [4] [4] = {{0, 1, 2, 3}, {0, 6, 2, 4}, {0, 6, 2, 4}, {0, 6, 2, 4}};
static const s32 CellList_s8_spaceship_offset [4] [4] [2] =
		{{{0, 0}, {-2, 0}, {-2, -2}, {0, -2}}, {{-1, 1}, {-5, -2}, {-2, -5}, {1, -1}}, {{-1, 1}, {-6, -3}, {-3, -6}, {1, -1}}, {{-1, 1}, {-7, -3}, {-3, -7}, {1, -1}}};
static const s32 CellList_s8_glider_dir [4] [4] = {{ 1,  1,  0,  1}, {-1,  1,  0, -1}, {-1, -1,  0,  1}, { 1, -1,  0, -1}};
static const s32 CellList_s8_xwss_dir [4] [4] = {{ 0,  1,  1,  0}, {-1,  0,  0,  1}, { 0, -1,  1,  0}, { 1,  0,  0,  1}};

static __not_inline void CellList_s8_init_transpose (GoLGrid *obj, int symmetry_case, s32 move_x, s32 move_y, CellList_s8_InitEnv *cie)
{
	if (!obj || !obj->grid || symmetry_case < 0 || symmetry_case > 7 || !cie)
		return (void) ffsc (__func__);
	
	Rect bb;
	GoLGrid_get_bounding_box (obj, &bb);
	
	GoLGrid_copy_unmatched_noinline (obj, &cie->zero_gg_1, -bb.left_x, -bb.top_y);
	
	if (symmetry_case & 4)
		GoLGrid_flip_diagonally_noinline (&cie->zero_gg_1, &cie->zero_gg_2);
	else
		GoLGrid_copy_noinline (&cie->zero_gg_1, &cie->zero_gg_2);
	
	if (symmetry_case & 2)
		GoLGrid_flip_vertically_noinline (&cie->zero_gg_2, &cie->zero_gg_1);
	else
		GoLGrid_copy_noinline (&cie->zero_gg_2, &cie->zero_gg_1);
	
	if (((symmetry_case & 3) == 1) || ((symmetry_case & 3) == 2))
		GoLGrid_flip_horizontally_noinline (&cie->zero_gg_1, &cie->zero_gg_2);
	else
		GoLGrid_copy_noinline (&cie->zero_gg_1, &cie->zero_gg_2);
	
	GoLGrid_copy_unmatched_noinline (&cie->zero_gg_2, obj, bb.left_x + move_x, bb.top_y + move_y);
}

static __not_inline void CellList_s8_init_evolve (GoLGrid *obj, s32 gen_count, CellList_s8_InitEnv *cie)
{
	if (!obj || !obj->grid || gen_count < 0 || !cie)
		return (void) ffsc (__func__);
	
	int gen;
	for (gen = 0; gen < gen_count; gen++)
	{
		GoLGrid_evolve_noinline (obj, &cie->temp_gg);
		GoLGrid_copy_noinline (&cie->temp_gg, obj);
	}
}

static __not_inline CellList_s8 *CellList_s8_init_store (const GoLGrid *gg, CellList_s8_InitEnv *cie)
{
	if (!gg || !gg->grid || !cie || cie->cell_list_ix >= CELLLIST_S8_STATIC_CELL_LIST_SIZE)
		return ffsc_p (__func__);
	
	CellList_s8 *cl = &CellList_s8_static_cell_list [cie->cell_list_ix];
	cie->cell_list_ix++;
	
	cl->cell_count = 0;
	cl->max_cells = CELLLIST_S8_STATIC_COORD_SIZE - cie->coord_ix;
	cl->cell = &CellList_s8_static_coord [cie->coord_ix];
	
	if (!GoLGrid_to_cell_list (gg, cl))
		return ffsc_p (__func__);
	
	cl->max_cells = cl->cell_count;
	cie->coord_ix += cl->cell_count;
	
	return cl;
}

static __not_inline void CellList_s8_init (void)
{
	if (CellList_s8_is_initialized)
		return (void) ffsc (__func__);
	
	CellList_s8_InitEnv cie;
	cie.cell_list_ix = 0;
	cie.coord_ix = 0;
	
	Rect mid_rect;
	Rect_make (&mid_rect, -32, -32, 64, 64);
	
	Rect zero_rect;
	Rect_make (&zero_rect, 0, 0, 64, 64);
	
	GoLGrid_create (&cie.temp_gg, &mid_rect);
	GoLGrid_create (&cie.zero_gg_1, &zero_rect);
	GoLGrid_create (&cie.zero_gg_2, &zero_rect);
	
	GoLGrid obj;
	GoLGrid_create (&obj, &mid_rect);
		
	GoLGrid_parse_life_history_simple ("2o$2o!", 0, 0, &obj);
	CellList_s8_block_cells = CellList_s8_init_store (&obj, &cie);
	
	GoLGrid_parse_life_history_simple ("o$o$o!", 0, 0, &obj);
	CellList_s8_blinker_cells [0] = CellList_s8_init_store (&obj, &cie);
	CellList_s8_init_evolve (&obj, 1, &cie);
	CellList_s8_blinker_cells [1] = CellList_s8_init_store (&obj, &cie);
	
	GoLGrid_parse_life_history_simple ("bo$obo$obo$bo!", 0, 0, &obj);
	CellList_s8_hive_cells [0] = CellList_s8_init_store (&obj, &cie);
	CellList_s8_init_transpose (&obj, 4, 0, 0, &cie);
	CellList_s8_hive_cells [1] = CellList_s8_init_store (&obj, &cie);
	
	int orientation;
	for (orientation = 0; orientation < 4; orientation++)
	{
		GoLGrid_parse_life_history_simple ("b2o$o2bo$obo$bo!", 0, 0, &obj);
		CellList_s8_init_transpose (&obj, orientation, 0, 0, &cie);
		CellList_s8_loaf_cells [orientation] = CellList_s8_init_store (&obj, &cie);
		
		GoLGrid_parse_life_history_simple ("2o$obo$bo!", 0, 0, &obj);
		CellList_s8_init_transpose (&obj, orientation, 0, 0, &cie);
		CellList_s8_boat_cells [orientation] = CellList_s8_init_store (&obj, &cie);
	}
	
	GoLGrid_parse_life_history_simple ("b2o$o2bo$o2bo$b2o!", 0, 0, &obj);
	CellList_s8_pond_cells = CellList_s8_init_store (&obj, &cie);
	
	int size;
	int dir;
	int phase;
	
	for (size = 0; size < 4; size++)
		for (dir = 0; dir < 4; dir++)
			for (phase = 0; phase < 4; phase++)
			{
				GoLGrid_parse_life_history_simple (CellList_s8_spaceship_spec [size], 0, 0, &obj);
				CellList_s8_init_transpose (&obj, CellList_s8_spaceship_orientation [size] [dir], CellList_s8_spaceship_offset [size] [dir] [0], CellList_s8_spaceship_offset [size] [dir] [1], &cie);
				CellList_s8_init_evolve (&obj, 4 - phase, &cie);
				CellList_s8_spaceship_cells [size] [dir] [phase] = CellList_s8_init_store (&obj, &cie);
			}
	
	GoLGrid_free (&cie.zero_gg_2);
	GoLGrid_free (&cie.zero_gg_1);
	GoLGrid_free (&cie.temp_gg);
	GoLGrid_free (&obj);
	
	CellList_s8_is_initialized = TRUE;
}

static __force_inline const CellList_s8 *CellList_s8_get_block_cells (void)
{
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	return CellList_s8_block_cells;
}

static __force_inline const CellList_s8 *CellList_s8_get_blinker_cells (int phase)
{
	if (phase < 0 || phase > 1)
		return ffsc_p (__func__);
	
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	return CellList_s8_blinker_cells [phase];
}

static __force_inline const CellList_s8 *CellList_s8_get_hive_cells (int orientation)
{
	if (orientation < 0 || orientation > 1)
		return ffsc_p (__func__);
	
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	return CellList_s8_hive_cells [orientation];
}

static __force_inline const CellList_s8 *CellList_s8_get_loaf_cells (int orientation)
{
	if (orientation < 0 || orientation > 3)
		return ffsc_p (__func__);
	
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	return CellList_s8_loaf_cells [orientation];
}

static __force_inline const CellList_s8 *CellList_s8_get_boat_cells (int orientation)
{
	if (orientation < 0 || orientation > 3)
		return ffsc_p (__func__);
	
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	return CellList_s8_boat_cells [orientation];
}

static __force_inline const CellList_s8 *CellList_s8_get_pond_cells (void)
{
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	return CellList_s8_pond_cells;
}

static __force_inline const CellList_s8 *CellList_s8_get_glider_cells (const Glider *gl, s32 generation, s32 *x_to_add, s32 *y_to_add)
{
	if (!gl || gl->dir < 0 || gl->dir > 3 || !x_to_add || !y_to_add)
	{
		if (x_to_add)
			*x_to_add = 0;
		if (y_to_add)
			*y_to_add = 0;
		
		return ffsc_p (__func__);
	}
	
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	s32 gen_0_timing = gl->timing - generation;
	*x_to_add = CellList_s8_glider_dir [gl->dir] [0] * (gen_0_timing >> 2);
	*y_to_add = (CellList_s8_glider_dir [gl->dir] [1] * (gen_0_timing >> 2)) - (CellList_s8_glider_dir [gl->dir] [3] * gl->lane);
	
	return CellList_s8_spaceship_cells [0] [gl->dir] [gen_0_timing & 3];
}

static __not_inline const CellList_s8 *CellList_s8_get_spaceship_cells (const StandardSpaceship *ship, s32 generation, s32 *x_to_add, s32 *y_to_add)
{
	if (!ship || ship->size < 0 || ship->size > 3 || ship->dir < 0 || ship->dir > 3 || ship->parity < 0 || ship->parity > 1 || (ship->size == 0 && ship->parity != 0) || !x_to_add || !y_to_add)
	{
		if (x_to_add)
			*x_to_add = 0;
		if (y_to_add)
			*y_to_add = 0;
		
		return ffsc_p (__func__);
	}
	
	if (!CellList_s8_is_initialized)
		CellList_s8_init ();
	
	s32 gen_0_timing_with_parity = (ship->timing - generation) + (ship->parity << 1);
	if (ship->size == 0)
	{
		*x_to_add = CellList_s8_glider_dir [ship->dir] [0] * (gen_0_timing_with_parity >> 2);
		*y_to_add = (CellList_s8_glider_dir [ship->dir] [1] * (gen_0_timing_with_parity >> 2)) - (CellList_s8_glider_dir [ship->dir] [3] * ship->lane);
	}
	else
	{
		*x_to_add = (CellList_s8_xwss_dir [ship->dir] [0] * (((gen_0_timing_with_parity >> 2) << 1) - ship->parity)) + CellList_s8_xwss_dir [ship->dir] [2] * ship->lane;
		*y_to_add = (CellList_s8_xwss_dir [ship->dir] [1] * (((gen_0_timing_with_parity >> 2) << 1) - ship->parity)) + CellList_s8_xwss_dir [ship->dir] [3] * ship->lane;
	}
	
	return CellList_s8_spaceship_cells [ship->size] [ship->dir] [gen_0_timing_with_parity & 3];
}
