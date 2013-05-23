import os, platform, tempfile, shutil, mathutils

import codyn, camera, gui

from gi.repository import Cdn
import cdnrawc

def find_and_load_rawc(data):
    basename = os.path.basename(data.filename)

    if platform.system() == 'Darwin':
        ext = '.dylib'
    elif platform.system() == 'Windows':
        ext = '.dll'
    else:
        ext = '.so'

    name = os.path.splitext(basename)[0]
    rawclib = os.path.join(os.path.dirname(data.filename), 'lib{0}{1}'.format(name, ext))

    # Check if <rawclib> is newer
    try:
        rawctime = os.path.getmtime(rawclib)

        if rawctime > data.mtime:
            if not data.rawc is None:
                try:
                    os.remove(shutil.data.rawc.libname)
                except:
                    pass

            # Load rawc version of the network. We need a dirty trick to force
            # a reload
            f = tempfile.NamedTemporaryFile(delete=False)
            tmpname = f.name
            f.close()
            del f

            shutil.copy(rawclib, tmpname)

            data.rawc = cdnrawc.Network(name, tmpname)
            data.mtime = rawctime
        elif rawctime < data.mtime:
            data.rawc = None
    except OSError:
        data.rawc = None

class Simulator:
    class Force:
        def __init__(self, gobj, idx):
            self.gobj = gobj
            self.idx = idx

            for child in self.gobj.children:
                if 'bottom' in child.name:
                    self.bottom = child
                elif 'top' in child.name:
                    self.top = child

        def set_visible(self, v, obj=None):
            if obj is None:
                obj = self.gobj

            obj.visible = v

            for child in obj.children:
                self.set_visible(v, child)

        def update(self, f):
            visible = (abs(f[3]) + abs(f[4]) + abs(f[5]) > 0.01)
            self.set_visible(visible)

            if visible:
                vv = mathutils.Vector(f[3:6])
                norm = vv.normalized()
                l = vv.length

                n = mathutils.Vector([0, 0, 1])

                xyz = n.cross(norm)
                w = 1 + n.dot(norm)

                q = mathutils.Vector((w, xyz[0], xyz[1], xyz[2]))
                q /= q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]

                vl = l / 100

                self.gobj.worldOrientation = mathutils.Quaternion(list(q))
                self.bottom.localScale = [1, 1, vl]

                pz = (vl - 1) * 0.6923
                self.bottom.localPosition = [0, 0, pz]
                self.top.localPosition = [0, 0, pz]

    def __init__(self, obj, data):
        self.system = obj.children[0]
        self.data = data

        self.nodes = {}
        self.forces = {}

        self._find_nodes(self.system)
        self.setup()

    def setup(self):
        pass

    def _find_nodes(self, root):
        for child in root.children:
            if 'cdn_node' in child:
                self.nodes[child['cdn_node']] = child
            elif 'cdn_force' in child:
                parts = child['cdn_force'].split(':')
                self.forces[child['cdn_force']] = [child, parts[0], int(parts[1])]

            self._find_nodes(child)

class SimulatorCodyn(Simulator):
    class Node:
        def __init__(self, node, gobj):
            self.node = node
            self.gobj = gobj

            self.localMatrix = self.node.get_variable("localMatrix")

        def update(self):
            m = codyn.matrix_to_mat4x4(self.localMatrix.get_values())
            self.gobj.localTransform = m

    class Force(Simulator.Force):
        def __init__(self, node, gobj, idx):
            Simulator.Force.__init__(self, gobj, idx)

            self.node = node
            self.force = self.node.get_variable("forceAtLocations")

        def update(self):
            v = self.force.get_values()
            i = self.idx * 6

            vec = v.get_flat()[i:i + 6]
            Simulator.Force.update(self, vec)

    @property
    def t(self):
        return self.data.cdn.get_integrator().get_variable('t').get_value()

    def setup(self):
        self.cdn_nodes = []
        self.cdn_forces = []

        for n in self.nodes:
            self.cdn_nodes.append(SimulatorCodyn.Node(self.data.cdn.find_object(n), self.nodes[n]))

        for n in self.forces:
            f = self.forces[n]
            self.cdn_forces.append(SimulatorCodyn.Force(self.data.cdn.find_object(f[1]), f[0], f[2]))

    def step(self, t):
        dt = self.data.cdn.get_integrator().get_default_timestep()
        ns = max(1, int(t / dt))

        for i in range(0, ns):
            self.data.cdn.step(dt)

    def reset(self):
        self.data.cdn.reset()
        self.data.cdn.begin(0)

    def update(self):
        # Update game node transformation from local transform
        for node in self.cdn_nodes:
            node.update()

        for force in self.cdn_forces:
            force.update()

class SimulatorRawc(Simulator):
    class Node:
        def __init__(self, node, gobj):
            self.node = node
            self.gobj = gobj

            self.localMatrix = self.node["localMatrix"]

        def update(self):
            m = codyn.to_mat4x4(self.localMatrix.flat_value,
                                 self.localMatrix.dimension)

            self.gobj.localTransform = m

    class Force(Simulator.Force):
        def __init__(self, node, gobj, idx):
            Simulator.Force.__init__(self, gobj, idx)

            self.node = node
            self.force = self.node["forceAtLocations"]

        def update(self):
            i = self.idx * 6
            f = self.force.flat_value

            Simulator.Force.update(self, f[i:i + 6])

    def setup(self):
        self.cdn_nodes = []
        self.cdn_forces = []

        for n in self.nodes:
            mn = self.data.rawc.topology.fullname_to_node[n]
            node = SimulatorRawc.Node(mn, self.nodes[n])

            self.cdn_nodes.append(node)

        for n in self.forces:
            force = self.forces[n]

            mn = self.data.rawc.topology.fullname_to_node[force[1]]
            force = SimulatorRawc.Force(mn, force[0], force[2])

            self.cdn_forces.append(force)

    def reset(self):
        self.data.rawc.reset(0)

    def update(self):
        for node in self.cdn_nodes:
           node.update()

        for force in self.cdn_forces:
            force.update()

    def step(self, t):
        dt = self.data.rawc.default_timestep
        ns = max(1, int(t / dt))

        for i in range(0, ns):
            self.data.rawc.step(dt)

def setup_gui(data):
    b = gui.Box(orientation=gui.Box.HORIZONTAL)
    b.background = gui.BoxTexture('gui.atlas:bar', [-1], [-1, 1])
    b.padding = gui.Rect(6, 6, 6, 6)
    b.alignment.y = 1
    b.fill.x = True
    b.homogeneous = False
    b.spacing = 6

    play = gui.Button(icon='gui.atlas:play')
    b.add(play)

    step = gui.Button(icon='gui.atlas:step')
    b.add(step)

    la = gui.Widget(alignment=gui.Alignment(1, 0.5), fill=gui.Fill(True, True))

    l = gui.Label(text='time: {:>6.3f}'.format(0))
    l.color = gui.Color(1, 1, 1, 1)
    l.padding = gui.Rect.uniform(3)
    l.alignment = gui.Alignment(1, 0.5)

    la.add(l)
    b.add(la)

    data.gui.add(b)
    data.lbl_time = l

def init():
    import bge

    cont = bge.logic.getCurrentController()
    owner = cont.owner

    filename = owner['cdn_filename']
    mtime = os.path.getmtime(filename)

    if not owner.name in codyn.data.networks or \
       mtime > codyn.data.networks[owner.name].mtime:
        # Load network from property
        network = Cdn.Network.new_from_path(filename)
        network.compile(None, None)

        data = codyn.data.networks[owner.name]

        data.cdn = network
        data.filename = filename
        data.mtime = mtime
        data.rawc = None

    # Load rawc version if it's there
    data = codyn.data.networks[owner.name]
    find_and_load_rawc(data)

    if not data.rawc is None:
        data.simulator = SimulatorRawc(owner, data)
    else:
        data.simulator = SimulatorCodyn(owner, data)

    data.simulator.reset()
    data.simulator.update()
    data.paused = False
    data.gui = gui.Screen()

    setup_gui(data)

    scene = bge.logic.getCurrentScene()
    scene.post_draw = [data.gui.draw]

    bge.render.showMouse(True)
    cont.activate('init_actuator')

def loop():
    import bge

    cont = bge.logic.getCurrentController()
    owner = cont.owner

    fps = bge.logic.getLogicTicRate()
    data = codyn.data.networks[owner.name]

    if not data.paused:
        data.simulator.step(1.0 / fps)
        data.simulator.update()

    data.lbl_time.text = 'time: {:>6.3f}'.format(data.simulator.t)

    camera.update(bge.logic.getCurrentScene().active_camera)

    if bge.events.SPACEKEY in bge.logic.keyboard.active_events:
        sp = bge.logic.keyboard.active_events[bge.events.SPACEKEY]

        if sp == bge.logic.KX_INPUT_JUST_RELEASED:
            data.paused = not data.paused

# vi:ts=4:et
