static __force_inline int GoLGrid_to_cell_list (const GoLGrid *gg, CellList_s8 *cl)
{
	if (cl)
		cl->cell_count = 0;
	
	if (!gg || !gg->grid || !cl || cl->max_cells < 0)
		return ffsc (__func__);
	
	if (gg->pop_x_off <= gg->pop_x_on)
		return TRUE;
	
	s32 col_on = gg->pop_x_on >> 6;
	s32 col_off = (gg->pop_x_off + 63) >> 6;
	
	s32 cell_ix = 0;
	s32 col_ix;
	s32 row_ix;
	
	for (col_ix = col_on; col_ix < col_off; col_ix++)
		for (row_ix = gg->pop_y_on; row_ix < gg->pop_y_off; row_ix++)
		{
			u64 grid_word = gg->grid [(gg->col_offset * (u64) col_ix) + (u64) row_ix];
			while (grid_word != 0)
			{
				s32 first_bit = most_significant_bit_u64 (grid_word);
				grid_word &= ~(((u64) 1u) << first_bit);
				
				s32 cell_x = gg->grid_rect.left_x + (64 * col_ix) + (63 - first_bit);
				s32 cell_y = gg->grid_rect.top_y + row_ix;
				
				if (cell_x < -128 || cell_x > 127 || cell_y < -128 || cell_y > 127 || cell_ix >= cl->max_cells)
				{
					cl->cell_count = 0;
					return FALSE;
				}
				
				cl->cell [cell_ix].x = cell_x;
				cl->cell [cell_ix].y = cell_y;
				cell_ix++;
			}
		}
	
	cl->cell_count = cell_ix;
	return TRUE;
}

static __not_inline int GoLGrid_to_cell_list_noinline (const GoLGrid *gg, CellList_s8 *cl)
{
	return GoLGrid_to_cell_list (gg, cl);
}

static __not_inline int GoLGrid_or_text_pattern (GoLGrid *gg, const char *pattern, int left_x, int top_y)
{
	int not_clipped = TRUE;
	s32 text_ix = 0;
	s32 cur_y = top_y;
	s32 cur_x = left_x;
	
	while (TRUE)
	{
		char c = pattern [text_ix++];
		
		if (c == '\0')
			break;
		else if (c == '\n')
		{
			cur_y++;
			cur_x = left_x;
		}
		else
		{
			if (c == ' ' || c == '.')
				; // Do nothing
			else if (c == '*' || c == '@')
				not_clipped &= GoLGrid_set_cell_on (gg, cur_x, cur_y);
			else
				fprintf (stderr, "Illegal character in %s\n", __func__);
			
			cur_x++;
		}
	}
	
	return not_clipped;
}

static __force_inline int GoLGrid_or_cell_list (GoLGrid *gg, const CellList_s8 *cl, s32 x_offs, s32 y_offs)
{
	if (!gg || !gg->grid || !cl)
		return ffsc (__func__);
	
	int not_clipped = TRUE;
	s32 cell_ix;
	for (cell_ix = 0; cell_ix < cl->cell_count; cell_ix++)
		not_clipped &= GoLGrid_set_cell_on (gg, cl->cell [cell_ix].x + x_offs, cl->cell [cell_ix].y + y_offs);
	
	return not_clipped;
}

static __force_inline int GoLGrid_or_glider (GoLGrid *gg, const Glider *gl)
{
	if (!gg || !gg->grid || !gl)
		return ffsc (__func__);
	
	s32 x_offs;
	s32 y_offs;
	const CellList_s8 *glider_cells = CellList_s8_get_glider_cells (gl, gg->generation, &x_offs, &y_offs);
	
	if (!glider_cells)
		return ffsc (__func__);
	
	return GoLGrid_or_cell_list (gg, glider_cells, x_offs, y_offs);
}

static __not_inline int GoLGrid_or_filled_circle (GoLGrid *gg, double cent_x, double cent_y, double radius)
{
	if (!gg || !gg->grid || radius < 0.0)
		return ffsc (__func__);
	
	s32 y_on = round_double (cent_y - radius);
	s32 y_off = 1 + round_double (cent_y + radius);
	s32 x_on = round_double (cent_x - radius);
	s32 x_off = 1 + round_double (cent_x + radius);
	
	int not_clipped = TRUE;
	s32 y;
	s32 x;
	
	for (y = y_on; y < y_off; y++)
		for (x = x_on; x < x_off; x++)
			if (((double) x - cent_x) * ((double) x - cent_x) + ((double) y - cent_y) * ((double) y - cent_y) < (radius * radius))
				not_clipped &= GoLGrid_set_cell_on (gg, x, y);
	
	return not_clipped;
}

static __not_inline void GoLGrid_print (const GoLGrid *gg)
{
	if (!gg || !gg->grid)
		return (void) ffsc (__func__);
	
	if (GoLGrid_is_empty (gg))
	{
		printf ("--- Empty grid\n\n");
		return;
	}
	
	Rect pr;
	GoLGrid_get_bounding_box (gg, &pr);
	Rect_add_borders (&pr, 2);
	
	int y;
	int x;
	for (y = pr.top_y; y < pr.top_y + pr.height; y++)
	{
		for (x = pr.left_x; x < pr.left_x + pr.width; x++)
			printf ("%c", (GoLGrid_get_cell (gg, x, y) != 0) ? '@' : '.');
		
		printf ("\n");
	}
	
	printf ("\n");
}

static __not_inline void GoLGrid_int_print_life_history_symbol (FILE *stream, char symbol, s32 count, int *line_length)
{
	if (!stream || !line_length)
		return (void) ffsc (__func__);
	
	if (count == 0)
		return;
	else if (count == 1)
	{
		fprintf (stream, "%c", symbol);
		(*line_length)++;
	}
	else if (count > 1)
	{
		fprintf (stream, "%d%c", count, symbol);
		(*line_length) += (1 + digits_in_u32 ((u32) count));
	}
	
	if (*line_length > 68)
	{
		fprintf (stream, "\n");
		(*line_length) = 0;
	}
}

static __not_inline void GoLGrid_print_life_history_full (FILE *stream, const Rect *print_rect, const GoLGrid *on_gg, const GoLGrid *marked_gg, const GoLGrid *envelope_gg, const GoLGrid *special_gg)
{
	if ((print_rect != NULL && (print_rect->width < 0 || print_rect->height < 0)) || (!on_gg && !marked_gg && !envelope_gg && !special_gg) ||
			(on_gg && !on_gg->grid) || (marked_gg && !marked_gg->grid) || (envelope_gg && !envelope_gg->grid) || (special_gg && !special_gg->grid))
		return (void) ffsc (__func__);
	
	if (!stream)
		stream = stdout;
	
	const Rect *pr = (print_rect ? print_rect : &on_gg->grid_rect);
	
// FIXME: What if the grid rects are different?
	
	fprintf (stream, "x = %d, y = %d, rule = LifeHistory\n", pr->width, pr->height);
	
	int line_length = 0;
	int unwritten_cell_state = 0;
	s32 unwritten_cell_count = 0;
	s32 unwritten_newline_count = 0;
	
	s32 y;
	s32 x;
	for (y = pr->top_y; y < pr->top_y + pr->height; y++)
	{
		for (x = pr->left_x; x < pr->left_x + pr->width; x++)
		{
			int cell_state = 0;
			
			if (on_gg && GoLGrid_get_cell (on_gg, x, y))
				cell_state = 1;
			
			if (marked_gg && GoLGrid_get_cell (marked_gg, x, y))
				cell_state = ((cell_state == 1) ? 3 : 4);
			
			if (special_gg && GoLGrid_get_cell (special_gg, x, y))
				cell_state = ((cell_state == 1 || cell_state == 3) ? 5 : 6);
			
			if (envelope_gg && cell_state == 0 && GoLGrid_get_cell (envelope_gg, x, y))
				cell_state = 2;
			
			if (unwritten_newline_count > 0 && cell_state != 0)
			{
				GoLGrid_int_print_life_history_symbol (stream, '$', unwritten_newline_count, &line_length);
				unwritten_newline_count = 0;
			}
			
			if (unwritten_cell_count > 0 && cell_state != unwritten_cell_state)
			{
				char symbol = (unwritten_cell_state == 0 ? '.' : 'A' + (unwritten_cell_state - 1));
				GoLGrid_int_print_life_history_symbol (stream, symbol, unwritten_cell_count, &line_length);
				
				unwritten_cell_count = 0;
			}
			
			unwritten_cell_state = cell_state;
			unwritten_cell_count++;
		}
		
		if (unwritten_cell_count > 0 && unwritten_cell_state != 0)
			GoLGrid_int_print_life_history_symbol (stream, 'A' + (unwritten_cell_state - 1), unwritten_cell_count, &line_length);
		
		unwritten_cell_count = 0;
		unwritten_newline_count++;
	}
	
	fprintf (stream, "!\n");
}

static __not_inline void GoLGrid_print_life_history (const GoLGrid *on_gg)
{
	GoLGrid_print_life_history_full (stdout, NULL, on_gg, NULL, NULL, NULL);
}

static __not_inline int GoLGrid_int_get_life_history_symbol (const char **lh, int *success, int *state, s32 *count)
{
	if (success)
		*success = FALSE;
	if (state)
		*state = 0;
	if (count)
		*count = 0;
	
	if (!lh || !*lh || !success || !state || !count)
		return ffsc (__func__);
	
	while (TRUE)
	{
		char c = **lh;
		
		if (c == '!' || c == '\0')
		{
			*success = TRUE;
			return FALSE;
		}
		
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
		{
			(*lh)++;
			continue;
		}
		
		s32 cnt = 1;
		if (c >= '0' && c <= '9')
		{
			u64 cnt_u64;
			if (!parse_u64 (lh, &cnt_u64) || cnt_u64 > s32_MAX)
				return FALSE;
			
			cnt = cnt_u64;
			c = **lh;
		}
		
		if (c == '$')
			*state = -1;
		else if (c == '.' || c == 'b')
			*state = 0;
		else if (c == 'o')
			*state = 1;
		else if (c >= 'A' && c <= 'F')
			*state = 1 + (c - 'A');
		else
			return FALSE;
		
		(*lh)++;
		*success = TRUE;
		*count = cnt;
		return TRUE;
	}
}

static __not_inline int GoLGrid_parse_life_history (const char *lh, s32 left_x, s32 top_y, GoLGrid *on_gg, GoLGrid *marked_gg, GoLGrid *envelope_gg, GoLGrid *special_gg, int *clipped, int *reinterpreted)
{
// FIXME: Find a safer way to check for overflow

	if (clipped)
		*clipped = FALSE;
	
	if (reinterpreted)
		*reinterpreted = FALSE;
	
	if (!lh || (!on_gg && !marked_gg && !envelope_gg && !special_gg) || (on_gg && !on_gg->grid) || (marked_gg && !marked_gg->grid) || (envelope_gg && !envelope_gg->grid) || (special_gg && !special_gg->grid))
		return ffsc (__func__);
	
	if (on_gg)
		GoLGrid_clear_noinline (on_gg);
	if (marked_gg)
		GoLGrid_clear_noinline (marked_gg);
	if (envelope_gg)
		GoLGrid_clear_noinline (envelope_gg);
	if (special_gg)
		GoLGrid_clear_noinline (special_gg);
	
	int used_state [7];
	int state_ix;
	for (state_ix = 0; state_ix <= 6; state_ix++)
		used_state [state_ix] = FALSE;
	
	s32 cur_x = left_x;
	s32 cur_y = top_y;
	
	int overflow_x = FALSE;
	int overflow_y = FALSE;
	int not_clipped = TRUE;
	int success;
	
	while (TRUE)
	{
		int state;
		s32 count;
		
		if (!GoLGrid_int_get_life_history_symbol (&lh, &success, &state, &count))
			break;
		
		if (count == 0)
			continue;
		
		if (state == -1)
		{
			cur_x = left_x;
			overflow_x = FALSE;
			
			s32 old_y = cur_y;
			cur_y += count;
			if (cur_y < old_y)
				overflow_y = TRUE;
		}
		else if (state == 0)
		{
			s32 old_x = cur_x;
			cur_x += count;
			if (cur_x < old_x)
				overflow_x = TRUE;
		}
		else
		{
			s32 count_ix;
			for (count_ix = 0; count_ix < count; count_ix++)
			{
				if (overflow_y || overflow_x)
				{
					not_clipped = FALSE;
					break;
				}
				else
				{
					used_state [state] = TRUE;
					
					if (on_gg && (state == 1 || state == 3 || state == 5))
						not_clipped &= GoLGrid_set_cell_on (on_gg, cur_x, cur_y);
					
					if (marked_gg && (state == 3 || state == 4 || state == 5))
						not_clipped &= GoLGrid_set_cell_on (marked_gg, cur_x, cur_y);
					
					if (envelope_gg && (state == 2))
						not_clipped &= GoLGrid_set_cell_on (envelope_gg, cur_x, cur_y);
					
					if (special_gg && (state == 5 || state == 6))
						not_clipped &= GoLGrid_set_cell_on (special_gg, cur_x, cur_y);
				}
				
				cur_x++;
				if (cur_x <= left_x)
					overflow_x = TRUE;
			}
		}
	}
	
	if (!success)
	{
		if (clipped)
			*clipped = FALSE;
		
		if (on_gg)
			GoLGrid_clear_noinline (on_gg);
		if (marked_gg)
			GoLGrid_clear_noinline (marked_gg);
		if (envelope_gg)
			GoLGrid_clear_noinline (envelope_gg);
		if (special_gg)
			GoLGrid_clear_noinline (special_gg);
		
		return FALSE;
	}
	
	if (clipped)
		*clipped = !not_clipped;
	
	if (reinterpreted)
	{
		if (!on_gg && (used_state [1] || used_state [3] || used_state [5]))
			*reinterpreted = TRUE;
		if (!marked_gg && (used_state [3] || used_state [4] || (!special_gg && used_state [3] && used_state [5])))
			*reinterpreted = TRUE;
		if (!envelope_gg && used_state [2])
			*reinterpreted = TRUE;
		if (!special_gg && used_state [6])
			*reinterpreted = TRUE;
	}
	
	return TRUE;
}

static __not_inline int GoLGrid_parse_life_history_simple (const char *lh, s32 left_x, s32 top_y, GoLGrid *on_gg)
{
	return GoLGrid_parse_life_history (lh, left_x, top_y, on_gg, NULL, NULL, NULL, NULL, NULL);
}
