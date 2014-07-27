/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/versioning_defaults.c
 *  \ingroup blenloader
 */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "BLO_readfile.h"


/**
 * Override values in in-memory startup.blend, avoids resaving for small changes.
 */
void BLO_update_defaults_userpref_blend(void)
{
	/* defaults from T37518 */

	U.uiflag |= USER_ZBUF_CURSOR;
	U.uiflag |= USER_QUIT_PROMPT;
	U.uiflag |= USER_CONTINUOUS_MOUSE;

	U.versions = 1;
	U.savetime = 2;
}

/**
 * Update defaults in startup.blend, without having to save and embed the file.
 * This function can be emptied each time the startup.blend is updated. */
void BLO_update_defaults_startup_blend(Main *bmain)
{
	Scene *scene;
	SceneRenderLayer *srl;
	FreestyleLineStyle *linestyle;
	Mesh *me;
	Material *mat;

	for (scene = bmain->scene.first; scene; scene = scene->id.next) {
		scene->r.im_format.planes = R_IMF_PLANES_RGBA;
		scene->r.im_format.compress = 15;

		for (srl = scene->r.layers.first; srl; srl = srl->next) {
			srl->freestyleConfig.sphere_radius = 0.1f;
			srl->pass_alpha_threshold = 0.5f;
		}

		if (scene->toolsettings) {
			ToolSettings *ts = scene->toolsettings;

			if (ts->sculpt) {
				Sculpt *sculpt = ts->sculpt;
				sculpt->paint.symmetry_flags |= PAINT_SYMM_X;
				sculpt->flags |= SCULPT_DYNTOPO_COLLAPSE;
				sculpt->detail_size = 12;
			}
		}
	}

	for (linestyle = bmain->linestyle.first; linestyle; linestyle = linestyle->id.next) {
		linestyle->flag = LS_SAME_OBJECT | LS_NO_SORTING | LS_TEXTURE;
		linestyle->sort_key = LS_SORT_KEY_DISTANCE_FROM_CAMERA;
		linestyle->integration_type = LS_INTEGRATION_MEAN;
		linestyle->texstep = 1.0;
	}

	{
		bScreen *screen;

		for (screen = bmain->screen.first; screen; screen = screen->id.next) {
			ScrArea *area;
			for (area = screen->areabase.first; area; area = area->next) {
				SpaceLink *space_link;
				for (space_link = area->spacedata.first; space_link; space_link = space_link->next) {
					if (space_link->spacetype == SPACE_CLIP) {
						SpaceClip *space_clip = (SpaceClip *) space_link;
						space_clip->flag &= ~SC_MANUAL_CALIBRATION;
					}
					else if (space_link->spacetype == SPACE_BUTS) {
						ARegion *ar, *arhead;
						ListBase *lb;

						if (space_link == area->spacedata.first) {
							lb = &area->regionbase;
						}
						else {
							lb = &space_link->regionbase;
						}

						/* if the header gets flipped to the top, we want it to be above the tabs! */
						for (arhead = lb->first; arhead; arhead = arhead->next) {
							if (arhead->regiontype == RGN_TYPE_HEADER) {
								arhead->alignment = RGN_ALIGN_BOTTOM;
								break;
							}
						}

						ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
						BLI_insertlinkafter(lb, arhead, ar);
						ar->regiontype = RGN_TYPE_TABS;
						ar->alignment = RGN_ALIGN_TOP;

						{
							View2D *v2d = &ar->v2d;

							v2d->keepzoom = (V2D_KEEPASPECT | V2D_LIMITZOOM | V2D_KEEPZOOM);
							v2d->minzoom = ar->v2d.maxzoom = 1.0f;

							v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
							v2d->keeptot = V2D_KEEPTOT_STRICT;

							/* no scrollers! */
							v2d->scroll = 0;
						}
					}
				}
			}
		}
	}

	for (me = bmain->mesh.first; me; me = me->id.next) {
		me->smoothresh = DEG2RADF(180.0f);
		me->flag &= ~ME_TWOSIDED;
	}

	for (mat = bmain->mat.first; mat; mat = mat->id.next) {
		mat->line_col[0] = mat->line_col[1] = mat->line_col[2] = 0.0f;
		mat->line_col[3] = 1.0f;
	}

	{
		Brush *br;
		br = BKE_brush_add(bmain, "Fill");
		br->imagepaint_tool = PAINT_TOOL_FILL;
		br->ob_mode = OB_MODE_TEXTURE_PAINT;

		br = (Brush *)BKE_libblock_find_name_ex(bmain, ID_BR, "Mask");
		if (br) {
			br->imagepaint_tool = PAINT_TOOL_MASK;
			br->ob_mode |= OB_MODE_TEXTURE_PAINT;
		}
	}
}

