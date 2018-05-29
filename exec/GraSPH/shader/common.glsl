#pragma once

// buffer bindings
#define RENDERER_POSITION_BUFFER_BINDING 0

#define PARTICLE_BUFFER_BINDING 2
#define PARTICLE_POSITION_BUFFER_BINDING 2
#define PARTICLE_VELOCITY_BUFFER_BINDING 3
#define PARTICLE_ACCELERATION_BUFFER_BINDING 4
#define PARTICLE_HYDRO_BUFFER_BINDING 5
#define PARTICLE_SMLENGTH_BUFFER_BINDING 6
#define PARTICLE_TIMESTEP_BUFFER_BINDING 7
#define VERLET_BUFFER_BINDING 8

// arrays for rendering data
#define RENDERER_POSITION_ARRAY 0
#define RENDERER_MASS_ARRAY 1

// defines for buffer access

// position buffer
#define POSITION xyz
#define MASS w

// velocity buffer
#define VELOCITY xyz

// acceleration buffer
#define ACCEL xyz
#define MAXVSIG w

// hydro buffer
#define DENSITY x
#define PRESSURE y
#define SPEED_OF_SOUND z
#define DRHO_DH w