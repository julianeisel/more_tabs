# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Header, Panel


class PROPERTIES_HT_header(Header):
    bl_space_type = 'PROPERTIES'

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        row = layout.row()
        row.template_header()
        
class USERPREF_PT_tabs(Panel):
    bl_label = ""
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'TABS'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        layout.prop_tabs(view, "context", icon_only=True, separate=False)
        

def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
