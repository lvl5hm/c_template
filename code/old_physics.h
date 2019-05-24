
typedef struct {
  v3 dp;
  f32 d_angle;
} Velocity;

Velocity get_force(Transform t, v2 force_p, v2 force) {
  v2 p = v2_sub(force_p, t.p.xy);
  v2 center_axis = p;
  v2 center_normal = v2_perp(center_axis);
  
  v2 dp_add = v2_project(force, center_axis);
  f32 d_angle_add = v2_project_s(force, center_normal);
  
  Velocity result;
  result.dp = v2_to_v3(dp_add, 0);
  result.d_angle = d_angle_add;
  return result;
}


typedef struct {
  v2 v[16];
  i32 count;
} Polygon;

Polygon aabb_transform(rect2 box, Transform t) {
  //DEBUG_FUNCTION_BEGIN();
  Polygon result;
  result.v[0] = box.min;
  result.v[1] = V2(box.min.x, box.max.y);
  result.v[2] = box.max;
  result.v[3] = V2(box.max.x, box.min.y);
  result.count = 4;
  
  mat4x4 matrix4 = transform_apply(mat4x4_identity(), t);
  
  for (i32 i = 0; i < result.count; i++) {
    result.v[i] = mat4x4_mul_v4(matrix4, v2_to_v4(result.v[i], 1, 1)).xy;
  }
  
  //DEBUG_FUNCTION_END();
  return result;
}



typedef struct {
  v2 point;
  v2 normal;
  v2 intersect;
  b32 success;
} Collision;

void test_box_normals(Polygon a, Polygon b, Collision *result, f32 *min_intersect) {
  for (i32 vertex_index = 0; vertex_index < a.count; vertex_index++) {
    i32 begin_index = vertex_index;
    i32 end_index = vertex_index == a.count-1 ? 0 : vertex_index+1;
    v2 normal = v2_sub(a.v[end_index], a.v[begin_index]);
    
    Range a_range = inverted_infinity_range();
    Range b_range = inverted_infinity_range();
    v2 b_min = v2_zero();
    v2 b_max = v2_zero();
    
    for (i32 a_vertex_index = 0; a_vertex_index < a.count; a_vertex_index++) {
      v2 vertex = a.v[a_vertex_index];
      f32 dot = v2_dot(vertex, normal);
      if (dot < a_range.min) {
        a_range.min = dot;
      }
      if (dot > a_range.max) {
        a_range.max = dot;
      }
    }
    for (i32 b_vertex_index = 0; b_vertex_index < b.count; b_vertex_index++) {
      v2 vertex = b.v[b_vertex_index];
      f32 dot = v2_dot(vertex, normal);
      if (dot < b_range.min) {
        b_range.min = dot;
        b_max = vertex;
      }
      if (dot > b_range.max) {
        b_range.max = dot;
        b_min = vertex;
      }
    }
    
    f32 left_intersect = a_range.max - b_range.min;
    f32 right_intersect = b_range.max - a_range.min;
    
    if (right_intersect < 0 || left_intersect < 0) {
      result->success = false;
      break;
    }
    
    if (right_intersect >= 0) {
      if (right_intersect < *min_intersect) {
        *min_intersect = right_intersect;
        result->normal = normal;
        result->point = b_min;
      }
    }
    if (left_intersect >= 0) {
      if (left_intersect < *min_intersect) {
        *min_intersect = left_intersect;
        result->normal = normal;
        result->point = b_max;
      }
    }
  }
}

Collision collide_box_box(rect2 a_rect, Transform a_t, rect2 b_rect, Transform b_t) {
  Polygon a = aabb_transform(a_rect, a_t);
  Polygon b = aabb_transform(b_rect, b_t);
  Collision result = {0};
  result.success = true;
  
  f32 min_intersect_sqr = INFINITY;
  test_box_normals(a, b, &result, &min_intersect_sqr);
  if (!result.success) return result;
  
  test_box_normals(b, a, &result, &min_intersect_sqr);
  
  if (result.success) {
    result.intersect = v2_mul_s(result.normal, 
                                -min_intersect_sqr/v2_length_sqr(result.normal));
  }
  
  return result;
}


case Entity_Type_BOX: {
  //mat4x4 m = transform_apply(mat4x4_identity(), entity->t);
  //v2 mouse_p = mat4x4_mul_v4(m, v2_to_v4(mouse_meters, 0, 0)).xy;
  
#if 0        
  if (entity->index == 2) {
    entity->t.p = v2_to_v3(mouse_meters, 0);
    if (input->start.is_down) {
      entity->t.angle += 0.1f;
    }
    
    
  }
#endif
  if (entity_index == 1 && input->mouse.left.went_down) {
    state->selected_entity = entity_index;
    state->start_p = v2_sub(mouse_meters, entity->t.p.xy);
  }
  
  
  for (i32 other_index = entity_index+1; other_index < state->entity_count; other_index++) {
    Entity *other = get_entity(state, other_index);
    
    Collision collision = collide_box_box(entity->box_collider.rect,
                                          entity->t,
                                          other->box_collider.rect,
                                          other->t);
    if (collision.success) {
      push_rect(group, rect2_center_size(collision.point, V2(0.1f, 0.1f)));
      v2 axis = collision.normal;
      Velocity e_ch = {0};
      Velocity o_ch = {0};
      {
        v2 r = v2_sub(collision.point, entity->t.p.xy);
        v2 rot_vel = v2_mul_s(v2_perp(r), entity->d_angle);
        v2 total_vel = v2_add(entity->dp.xy, rot_vel);
        
        v2 force = v2_project(total_vel, axis);
        Velocity o_vel = get_force(other->t, collision.point, force);
        Velocity e_vel = get_force(entity->t, collision.point, v2_mul_s(force, -1));
        
        e_ch.dp = v3_add(e_ch.dp, e_vel.dp);
        e_ch.d_angle += e_vel.d_angle;
        o_ch.dp = v3_add(o_ch.dp, o_vel.dp);
        o_ch.d_angle += o_vel.d_angle;
        //entity->t.p = v3_add(entity->t.p, v2_to_v3(collision.intersect, 0));
      }
      {
        v2 r = v2_sub(collision.point, other->t.p.xy);
        v2 rot_vel = v2_mul_s(v2_perp(r), other->d_angle);
        v2 total_vel = v2_add(other->dp.xy, rot_vel);
        
        v2 force = v2_project(total_vel, axis);
        Velocity e_vel = get_force(entity->t, collision.point, force);
        Velocity o_vel = get_force(other->t, collision.point, 
                                   v2_mul_s(force, -1));
        
        e_ch.dp = v3_add(e_ch.dp, e_vel.dp);
        e_ch.d_angle += e_vel.d_angle;
        o_ch.dp = v3_add(o_ch.dp, o_vel.dp);
        o_ch.d_angle += o_vel.d_angle;
      }
      
      entity->dp = v3_add(e_ch.dp, entity->dp);
      entity->d_angle += e_ch.d_angle;
      
      other->dp = v3_add(o_ch.dp, other->dp);
      other->d_angle += o_ch.d_angle;
    }
  }
  
  entity->t.p = v3_add(entity->t.p, v3_mul_s(entity->dp, dt));
  entity->t.angle += entity->d_angle*dt;
} break;

case Entity_Type_ENEMY: {
  
  f32 FRICTION = 0.5f*dt;
  entity->dp = v3_mul_s(entity->dp, 1 - FRICTION);
  f32 r = entity->circle_collider.r*entity->t.scale.x;
  
  b32 do_bop = false;
  
  
  if (input->mouse.left.went_down && point_in_circle(mouse_meters, entity->t.p.xy, r)) {
    state->selected_entity = entity_index;
  }
  
  Entity *bopped = 0;
  for (i32 other_index = entity_index+1; other_index < state->entity_count; other_index++) {
    Entity *other = get_entity(state, other_index);
    if (other->type != Entity_Type_ENEMY) continue;
    
    f32 radius_sum = entity->circle_collider.r*entity->t.scale.x + other->circle_collider.r*other->t.scale.x;
    v2 diff = v2_sub(other->t.p.xy, entity->t.p.xy);
    f32 dist = v2_length(diff);
    
    if (dist < radius_sum) {
      do_bop = true;
      bopped = other;
      
      // NOTE(lvl5): collision happened
      v3 entity_new_dp = entity->dp;
      v3 other_new_dp = other->dp;
      v2 unit = v2_unit(diff);
      
      f32 overlap = dist - radius_sum;
      entity->t.p.xy = v2_add(entity->t.p.xy, v2_mul_s(unit, overlap));
      
      {
        v2 force = v2_project(entity->dp.xy, unit);
        other_new_dp.xy = v2_add(other_new_dp.xy, force);
        entity_new_dp.xy = v2_add(entity_new_dp.xy, 
                                  v2_mul_s(force, -1));
      }
      {
        v2 force = v2_project(other->dp.xy, unit);
        entity_new_dp.xy = v2_add(entity_new_dp.xy, force);
        other_new_dp.xy = v2_add(other_new_dp.xy, 
                                 v2_mul_s(force, -1));
      }
      
      other->dp = other_new_dp;
      entity->dp = entity_new_dp;
    }
  }
  
#define BOUNCE_C -0.7f
  if (entity->t.p.x + r > half_screen_meters.x) {
    entity->dp.x *= BOUNCE_C;
    entity->t.p.x = half_screen_meters.x-r;
    do_bop = true;
  }
  if (entity->t.p.x - r < -half_screen_meters.x) {
    entity->dp.x *= BOUNCE_C;
    entity->t.p.x = -half_screen_meters.x+r;
    do_bop = true;
  }
  
  if (entity->t.p.y + r > half_screen_meters.y) {
    entity->dp.y *= BOUNCE_C;
    entity->t.p.y = half_screen_meters.y-r;
    do_bop = true;
  }
  if (entity->t.p.y - r < -half_screen_meters.y) {
    entity->dp.y *= BOUNCE_C;
    entity->t.p.y = -half_screen_meters.y+r;
    do_bop = true;
  }
  
  entity->t.p = v3_add(entity->t.p, v3_mul_s(entity->dp, dt));
  entity->t.angle += entity->d_angle;
  
#if 1 
  if (do_bop) {
    Playing_Sound *snd = sound_play(&state->sound_state, &state->snd_bop, Sound_Type_EFFECTS);
#define MAX_BALL_SPEED 20
    v2 dp = entity->dp.xy;
    if (bopped) {
      dp = v2_add(bopped->dp.xy, dp);
    }
    
    f32 speed = v2_length(dp);
    f32 volume = speed/MAX_BALL_SPEED;
    sound_set_volume(snd, V2(volume, volume), 0);
    snd->speed = random_range(&state->rand, 0.95f, 1.08f);
  }
#endif
} break;


if (state->selected_entity) {
  Entity *e = get_entity(state, state->selected_entity);
  if (e->is_active) {
    v2 p = state->start_p;
    v2 rel_p = v2_add(e->t.p.xy, p);
    
    push_line(group, rel_p, mouse_meters, 0.03f);
    
    v2 force = v2_sub(mouse_meters, rel_p);
    push_line(group, rel_p, mouse_meters, 0.05f);
    
    v2 center_axis = p;
    v2 center_normal = v2_perp(center_axis);
    v2 dp_add = v2_project(force, center_axis);
    f32 d_angle_add = v2_project_s(force, center_normal);
    
    
    push_line(group, rel_p, e->t.p.xy, 0.08f);
    v2 d_angle_add_v = v2_project(force, center_normal);
    push_line(group, rel_p, v2_add(rel_p, center_normal), 0.08f);
    push_line_color(group, rel_p, v2_add(rel_p, dp_add), 0.01f, V4(0, 1, 0, 1));
    push_line_color(group, rel_p, v2_add(rel_p, d_angle_add_v), 0.01f, V4(0, 0, 1, 1));
    if (input->mouse.left.went_up) {
      Velocity v = get_force(e->t, p, force);
      e->dp = v3_add(v.dp, e->dp);
      e->d_angle += v.d_angle;
      state->selected_entity = 0;
    }
  }
}
