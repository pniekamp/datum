# datum
> Peter Niekamp, 2016 - 2018

### Vulkan Renderer
- Asset Pack Loading
- Resource Management
- Sprite Rendering
- Mesh Rendering
- Single Main Directional Lighting
- Clustered Deferred Based Lighting
- Main Light Cascaded Shadow Map
- Spot Light Parabolic Shadow Maps
- Screen Space Ambient Occlusion (SSAO)
- Hi-Z Screen Space Reflections (SSR)
- HDR Skybox & Bloom
- Physically Based Rendering (PBR) & Image Based Lighting (IBL)
- Spherical Harmonic Irradiance Probes
- Particle System
- Exponential Height Fog
- Water and FFT Ocean
- Skeletal Animation
- Deferred Decals
- Volumetric Fog
- Entity-Component Scene Management

![Datum](/bin/datumtest.png?raw=true "Datum")

### Building
Head branch is tested against gcc/clang on Linux and Mingw 5.3 on Windows.

Download [leap](https://github.com/pniekamp/leap) and [datum](https://github.com/pniekamp/datum) into a single directory (eg code/leap & code/datum)

#### GCC, CLANG
```
$ mkdir leap/build
$ pushd leap/build
$ cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
$ cmake --build . --target install/strip
$ popd
```
```
$ mkdir datum/build
$ pushd datum/build
$ cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEXAMPLES=ON ..
$ cmake --build . --target install/strip
$ popd
```

#### Visual Studio 15.7
```
$ mkdir leap/build
$ pushd leap/build
$ cmake -G "Visual Studio 15 2017 Win64" ..
$ cmake --build . --target install --config RelWithDebInfo
$ popd
```
```
$ mkdir datum/build
$ pushd datum/build
$ cmake -G "Visual Studio 15 2017 Win64" ..
$ cmake --build . --target install --config RelWithDebInfo
$ popd
```

There is also a [datum-vs branch](https://github.com/pniekamp/datum/tree/datum-vs) for VS2015 x64 Update 3.

![Datum](/bin/datumsponza.png?raw=true "Datum Sponza")
