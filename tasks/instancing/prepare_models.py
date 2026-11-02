import base64
import json
import math
import pathlib
import struct
import sys


ROOT = pathlib.Path(sys.argv[1])
ROOT.mkdir(parents=True, exist_ok=True)
COMPONENTS = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2),
              5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
WIDTHS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4,
          'MAT2': 4, 'MAT3': 9, 'MAT4': 16}


def identity():
    return [1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.]


def multiply(a, b):
    return [sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4))
            for c in range(4) for r in range(4)]


def node_matrix(node):
    if 'matrix' in node:
        return [float(v) for v in node['matrix']]
    x, y, z, w = node.get('rotation', [0., 0., 0., 1.])
    sx, sy, sz = node.get('scale', [1., 1., 1.])
    tx, ty, tz = node.get('translation', [0., 0., 0.])
    return [(1 - 2 * (y*y + z*z))*sx, 2*(x*y + z*w)*sx, 2*(x*z - y*w)*sx, 0.,
            2*(x*y - z*w)*sy, (1 - 2*(x*x + z*z))*sy, 2*(y*z + x*w)*sy, 0.,
            2*(x*z + y*w)*sz, 2*(y*z - x*w)*sz, (1 - 2*(x*x + y*y))*sz, 0.,
            tx, ty, tz, 1.]


def transform(m, p):
    x, y, z = p
    return (m[0]*x + m[4]*y + m[8]*z + m[12],
            m[1]*x + m[5]*y + m[9]*z + m[13],
            m[2]*x + m[6]*y + m[10]*z + m[14])


def cross(a, b):
    return (a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0])


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def normalize(v):
    n = math.sqrt(dot(v, v))
    return tuple(x / n for x in v) if n > 1e-10 else (0., 1., 0.)


def normal_transform(m, n):
    a = (m[0], m[1], m[2])
    b = (m[4], m[5], m[6])
    c = (m[8], m[9], m[10])
    inv_a, inv_b, inv_c = cross(b, c), cross(c, a), cross(a, b)
    det = dot(a, inv_a)
    if abs(det) < 1e-12:
        return (0., 1., 0.)
    return normalize(tuple((n[0]*inv_a[k] + n[1]*inv_b[k] + n[2]*inv_c[k]) / det
                           for k in range(3)))


def determinant(m):
    return dot((m[0], m[1], m[2]), cross((m[4], m[5], m[6]),
                                          (m[8], m[9], m[10])))


def convert(asset):
    model_dir = ROOT / asset
    model_path = model_dir / f'{asset}_1k.gltf'
    doc = json.loads(model_path.read_bytes())
    buffers = []
    for buf in doc['buffers']:
        uri = buf['uri']
        if uri.startswith('data:'):
            data = base64.b64decode(uri.split(',', 1)[1])
        else:
            data = (model_dir / uri).read_bytes()
        if len(data) < buf['byteLength']:
            raise ValueError(f'{asset}: incomplete glTF buffer')
        buffers.append(data)

    def accessor(index):
        a = doc['accessors'][index]
        if 'sparse' in a:
            raise ValueError(f'{asset}: sparse glTF accessor not supported')
        view = doc['bufferViews'][a['bufferView']]
        fmt, component_size = COMPONENTS[a['componentType']]
        width = WIDTHS[a['type']]
        stride = view.get('byteStride', component_size * width)
        offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        b = buffers[view['buffer']]
        unpack = struct.Struct('<' + fmt * width)
        return [unpack.unpack_from(b, offset + i * stride) for i in range(a['count'])]

    vertices = []
    indices = []
    scene = doc['scenes'][doc.get('scene', 0)]

    def visit(node_index, parent):
        node = doc['nodes'][node_index]
        matrix = multiply(parent, node_matrix(node))
        if 'mesh' in node:
            mesh = doc['meshes'][node['mesh']]
            for primitive in mesh['primitives']:
                if primitive.get('mode', 4) != 4:
                    raise ValueError(f'{asset}: unsupported primitive topology')
                attributes = primitive['attributes']
                positions = accessor(attributes['POSITION'])
                normals = accessor(attributes['NORMAL']) if 'NORMAL' in attributes else None
                primitive_indices = ([v[0] for v in accessor(primitive['indices'])]
                                     if 'indices' in primitive else list(range(len(positions))))
                if len(primitive_indices) % 3:
                    raise ValueError(f'{asset}: non-triangle mesh')
                if normals is None:
                    normals = [[0., 0., 0.] for _ in positions]
                    for j in range(0, len(primitive_indices), 3):
                        i0, i1, i2 = primitive_indices[j:j+3]
                        a, b, c = (positions[i] for i in (i0, i1, i2))
                        e1 = tuple(b[k]-a[k] for k in range(3))
                        e2 = tuple(c[k]-a[k] for k in range(3))
                        n = cross(e1, e2)
                        for i in (i0, i1, i2):
                            for k in range(3):
                                normals[i][k] += n[k]
                start = len(vertices)
                for p, n in zip(positions, normals):
                    vertices.append((transform(matrix, p), normal_transform(matrix, n)))
                flipped = determinant(matrix) < 0.
                for j in range(0, len(primitive_indices), 3):
                    tri = primitive_indices[j:j+3]
                    if flipped:
                        tri[1], tri[2] = tri[2], tri[1]
                    indices.extend(start + v for v in tri)
        for child in node.get('children', []):
            visit(child, matrix)

    for node in scene['nodes']:
        visit(node, identity())
    if not vertices or not indices:
        raise ValueError(f'{asset}: empty glTF mesh')

    bounds_min = [min(v[0][axis] for v in vertices) for axis in range(3)]
    bounds_max = [max(v[0][axis] for v in vertices) for axis in range(3)]
    center_x = (bounds_min[0] + bounds_max[0]) * 0.5
    center_z = (bounds_min[2] + bounds_max[2]) * 0.5
    extent = max(bounds_max[0] - bounds_min[0], bounds_max[2] - bounds_min[2], 1e-5)
    scale = (1.6 if asset == 'boulder_01' else 1.1) / extent
    result = ROOT / f'{asset}.mesh'
    with result.open('wb') as out:
        out.write(struct.pack('<4sII', b'RCK1', len(vertices), len(indices)))
        for p, n in vertices:
            out.write(struct.pack('<8f', (p[0] - center_x) * scale,
                                  (p[1] - bounds_min[1]) * scale,
                                  (p[2] - center_z) * scale, 1.,
                                  *normalize(n), 0.))
        out.write(struct.pack(f'<{len(indices)}I', *indices))
    print(f'{asset}: {len(vertices)} vertices, {len(indices) // 3} triangles')


for name in ('boulder_01', 'rock_09'):
    convert(name)
