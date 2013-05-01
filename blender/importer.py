import bpy, os, inspect, sys, math, mathutils
from gi.repository import Cdn

from . import codyn

class CodynImport(bpy.types.Operator):
    """Import a codyn model"""
    bl_idname = "object.codyn_import"
    bl_label = "Import Codyn Model"
    bl_description = "Import a codyn model"

    filepath = bpy.props.StringProperty(subtype="FILE_PATH")

    def make_object(self, context, name, mesh=None):
        if name in bpy.data.objects:
            ret = bpy.data.objects[name]

            if ret.users != 0:
                return ret
            else:
                ret.name = '__{0}'.format(name)

        if not mesh is None:
            mesh = mesh()

        ret = bpy.data.objects.new(name, mesh)
        context.scene.objects.link(ret)

        return ret

    def inertia_is_diag(self, inertia):
        return inertia[0][1] == 0 and \
               inertia[0][2] == 0 and \
               inertia[1][0] == 0 and \
               inertia[2][0] == 0 and \
               inertia[1][2] == 0 and \
               inertia[2][1] == 0

    def inertia_like_sphere(self, m, inertia):
        if not self.inertia_is_diag(inertia):
            return False

        if inertia[0][0] == inertia[1][1] and \
           inertia[1][1] == inertia[2][2]:
            return math.sqrt(inertia[0][0] / (2.0 / 5.0 * m))

        return False

    def inertia_like_cylinder(self, m, inertia):
        if not self.inertia_is_diag(inertia):
            return False

        f = 1.0 / 12.0 * m

        a = inertia[0][0] / f
        b = inertia[1][1] / f
        c = inertia[2][2] / (0.5 * m)

        r2 = c
        h2 = a - (3 * r2)

        if a == b and a != c and h2 > 0:
            return math.sqrt(r2), math.sqrt(h2)

        if w2 > 0 and d2 > 0 and h2 > 0:
            return math.sqrt(w2), math.sqrt(d2), math.sqrt(h2)

    def inertia_like_box(self, m, inertia):
        if not self.inertia_is_diag(inertia):
            return False

        f = 1.0 / 12.0 * m

        a = inertia[0][0] / f
        b = inertia[1][1] / f
        c = inertia[2][2] / f

        w2 = (c - a + b) / 2
        d2 = b - w2
        h2 = a - d2

        if w2 > 0 and d2 > 0 and h2 > 0:
            return math.sqrt(w2), math.sqrt(d2), math.sqrt(h2)

        return False

    def make_matrix(self, m):
        dim = m.dimension()
        v = m.get_flat()

        ret = []
        i = 0

        for c in range(0, dim.columns):
            for r in range(0, dim.rows):
                if c == 0:
                    ret.append([0] * dim.columns)

                ret[r][c] = v[i]
                i += 1

        return ret

    def try_make_shape(self, context, body):
        m = body.get_variable('m').get_value()
        I = body.get_variable('I').get_values()

        # Inertia matrix
        inertia = self.make_matrix(I)
        name = '{0}_shape'.format(body.get_id())

        r = self.inertia_like_sphere(m, inertia)

        if r:
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, size=r)
            context.active_object.name = name
            return context.active_object

        c = self.inertia_like_cylinder(m, inertia)

        if c:
            bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=c[0], depth=c[1])
            context.active_object.name = name
            return context.active_object

        b = self.inertia_like_box(m, inertia)

        if b:
            bpy.ops.mesh.primitive_cube_add()
            context.active_object.name = name
            context.active_object.scale = mathutils.Vector(b)
            bpy.ops.object.transform_apply(scale=True)

            return context.active_object
        else:
            return None

    def import_system(self, cdnobj, context, system):
        bodies = system.find_objects('has-template(physics.body)')
        ret = []

        # Dummy object for system
        sysobj = self.make_object(context, system.get_id(), lambda: bpy.data.meshes.new(system.get_id()))
        sysobj.parent = cdnobj
        ret.append(sysobj)

        objmap = {}
        nodemap = {}

        for body in bodies:
            # Center of mass
            com = body.get_variable("com")

            bid = body.get_id()
            obj = self.make_object(context, bid)

            if not 'cdn_node' in obj.game.properties:
                context.scene.objects.active = obj
                bpy.ops.object.game_property_new(type='STRING', name='cdn_node')

            obj.game.properties['cdn_node'].value = body.get_full_id()

            csid = '{0}_cs'.format(bid)

            cs = self.make_object(context, csid, lambda: bpy.data.meshes['coordinate_system'])
            cs.scale = [10, 10, 10]
            cs.parent = obj

            # Attach coordinate system visualization
            comid = '{0}_com'.format(bid)

            comobj = self.make_object(context, comid, lambda: bpy.data.meshes.new(comid))
            comobj.parent = obj

            shape = self.try_make_shape(context, body)

            if not shape is None:
                shape.location = com.get_values().get_flat()
                shape.parent = obj

                ret.append(shape)

            ret.append(obj)
            ret.append(cs)
            ret.append(comobj)

            objmap[obj] = body
            nodemap[body] = obj

            codyn.data.blender_to_node[obj] = body
            codyn.data.node_to_blender[body] = obj

            codyn.sync_variables_to_blender(obj)
            comobj.location = com.get_values().get_flat()

        pedge = system.find_object('templates-root . physics . joint')

        for body in bodies:
            obj = nodemap[body]

            for edge in body.get_edges():
                if pedge in edge.get_applied_templates() and edge.get_output() == body:
                    parnode = edge.get_input()

                    if not parnode in nodemap:
                        break

                    parobj = nodemap[parnode]

                    obj.parent = parobj
                    break

            if not obj.parent:
                obj.parent = sysobj

            # Transform
            nodemap[body].matrix_local = codyn.local_blender_transform(body)

        return ret

    def link_library(self):
        filename = inspect.getframeinfo(inspect.currentframe()).filename
        dirname = os.path.dirname(filename)

        with bpy.data.libraries.load(os.path.join(dirname, 'data', 'library.blend')) as (data_from, data_to):
            for attr in ['materials', 'meshes', 'objects', 'groups']:
                setattr(data_to, attr, getattr(data_from, attr))

    def execute(self, context):
        self.link_library()

        path = self.filepath
        path = '/home/jvanden/work/phd/codyn/codyn/examples/pendulum.cdn'

        network = Cdn.Network.new_from_path(path)
        err = Cdn.CompileError()

        if not network.compile(None, err):
            print(err)

        objmap = {}
        nodemap = {}
        objs = []

        name = os.path.basename(path)
        cdnobj = self.make_object(context, name)
        cdnobj['cdn_filename'] = path

        context.scene.objects.active = cdnobj

        if not 'init_sensor' in cdnobj.game.sensors:
            bpy.ops.logic.sensor_add(type='ALWAYS', name='init_sensor')

            inits = cdnobj.game.sensors['init_sensor']
            inits.use_tap = True

            bpy.ops.logic.controller_add(type='PYTHON', name='init_controller')

            ctrl = cdnobj.game.controllers['init_controller']
            ctrl.mode = 'MODULE'
            ctrl.module = 'codyn.simulator_init'
            ctrl.states = 1

            bpy.ops.logic.actuator_add(type='STATE', name='init_actuator')
            act = cdnobj.game.actuators['init_actuator']
            act.operation = 'SET'
            act.states[1] = True

            ctrl.link(sensor=inits, actuator=act)

            bpy.ops.logic.sensor_add(type='ALWAYS', name='loop_sensor')
            loops = cdnobj.game.sensors['loop_sensor']
            loops.use_pulse_true_level = True

            bpy.ops.logic.controller_add(type='PYTHON', name='loop_controller')
            ctrl = cdnobj.game.controllers['loop_controller']
            ctrl.states = 2
            ctrl.mode = 'MODULE'
            ctrl.module = 'codyn.simulator_loop'
            loops.link(ctrl)

            bpy.ops.object.game_property_new(type='STRING', name='cdn_filename')
            cdnobj.game.properties['cdn_filename'].value = path

        systems = network.find_objects('has-template(physics.system)')

        for system in systems:
            objs.extend(self.import_system(cdnobj, context, system))

        bpy.ops.object.select_all(action="DESELECT")

        for o in objs:
            o.select = True

        if len(objs):
            context.scene.objects.active = objs[0]

        context.scene.layers[0] = True
        context.scene.render.engine = 'BLENDER_GAME'

        codyn.data.networks[name].cdn = network
        codyn.data.networks[name].filename = path
        codyn.data.networks[name].mtime = os.path.getmtime(path)
        codyn.data.networks[name].rawc = None

        return {"FINISHED"}

    def invoke(self, context, event):
        #context.window_manager.fileselect_add(self)
        #return {'RUNNING_MODAL'}
        self.execute(context)
        return {'FINISHED'}

# vi:ts=4:et