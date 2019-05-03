
{
  Animation_Pack *pack = state->robot_parts + Robot_Part_RIGHT_LEG;
  pack->animation_count = 2;
  pack->animations = arena_push_array(&state->arena, Animation, 
                                      pack->animation_count);
  {
    Animation a;
    a.frames = sb_init(&state->arena, Animation_Frame, 16, true);
    
    Animation_Frame frame0 = (Animation_Frame){
      0,
      (Transform){
        V3(0, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame1 = (Animation_Frame){
      0.33f,
      (Transform){
        V3(0.2f, 0, 0), // p
        V3(1, 1, 1), // scale
        PI*0.4, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame2 = (Animation_Frame){
      0.66f,
      (Transform){
        V3(0.26f, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    sb_push(a.frames, frame0);
    sb_push(a.frames, frame1);
    sb_push(a.frames, frame2);
    
    frame0.position = 1.0f;
    sb_push(a.frames, frame0);
    
    a.frame_count = sb_count(a.frames);
    a.speed = 0.7f;
    pack->animations[Robot_Animation_WALK] = a;
  }
  
  {
    Animation a;
    a.frames = sb_init(&state->arena, Animation_Frame, 16, true);
    
    Animation_Frame frame0 = (Animation_Frame){
      0,
      (Transform){
        V3(0, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    Animation_Frame frame1 = (Animation_Frame){
      0.5f,
      (Transform){
        V3(0, -0.08f, 0), // p
        V3(1, 1, 1), // scale
        0.2f, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    sb_push(a.frames, frame0);
    sb_push(a.frames, frame1);
    
    frame0.position = 1.0f;
    sb_push(a.frames, frame0);
    a.frame_count = sb_count(a.frames);
    a.speed = 1;
    
    pack->animations[Robot_Animation_IDLE] = a;
  }
}


{
  Animation_Pack *pack = state->robot_parts + Robot_Part_BODY;
  pack->animation_count = 2;
  pack->animations = arena_push_array(&state->arena, Animation, 
                                      pack->animation_count);
  {
    Animation a;
    a.frames = sb_init(&state->arena, Animation_Frame, 16, true);
    
    Animation_Frame frame0 = (Animation_Frame){
      0,
      (Transform){
        V3(0.1f, 0, 0), // p
        V3(1, 1, 1), // scale
        0.2f, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame1 = (Animation_Frame){
      0.5f,
      (Transform){
        V3(0.1f, 0, 0), // p
        V3(1, 1, 1), // scale
        0.1f, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    sb_push(a.frames, frame0);
    sb_push(a.frames, frame1);
    
    frame0.position = 1.0f;
    sb_push(a.frames, frame0);
    
    a.frame_count = sb_count(a.frames);
    a.speed = 0.7f;
    pack->animations[Robot_Animation_WALK] = a;
  }
  
  {
    Animation a;
    a.frames = sb_init(&state->arena, Animation_Frame, 16, true);
    
    Animation_Frame frame0 = (Animation_Frame){
      0,
      (Transform){
        V3(0, 0.05f, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame1 = (Animation_Frame){
      0.5f,
      (Transform){
        V3(0, -0.05f, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    sb_push(a.frames, frame0);
    sb_push(a.frames, frame1);
    
    frame0.position = 1.0f;
    sb_push(a.frames, frame0);
    a.frame_count = sb_count(a.frames);
    a.speed = 1;
    
    pack->animations[Robot_Animation_IDLE] = a;
  }
}


{
  Animation_Pack *pack = state->robot_parts + Robot_Part_EYE;
  pack->animation_count = 2;
  pack->animations = arena_push_array(&state->arena, Animation, 
                                      pack->animation_count);
  {
    Animation a;
    a.frames = sb_init(&state->arena, Animation_Frame, 16, true);
    
    Animation_Frame frame0 = (Animation_Frame){
      0,
      (Transform){
        V3(0.07f, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame1 = (Animation_Frame){
      0.5f,
      (Transform){
        V3(0.09f, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    sb_push(a.frames, frame0);
    sb_push(a.frames, frame1);
    
    frame0.position = 1.0f;
    sb_push(a.frames, frame0);
    
    a.frame_count = sb_count(a.frames);
    a.speed = 1.5f;
    pack->animations[Robot_Animation_WALK] = a;
  }
  
  {
    Animation a;
    a.frames = sb_init(&state->arena, Animation_Frame, 16, true);
    
    Animation_Frame frame0 = (Animation_Frame){
      0,
      (Transform){
        V3(0, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame1 = (Animation_Frame){
      0.5f,
      (Transform){
        V3(0, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    Animation_Frame frame2 = (Animation_Frame){
      0.55f,
      (Transform){
        V3(-0.1f, 0.07f, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    Animation_Frame frame3 = (Animation_Frame){
      0.7f,
      (Transform){
        V3(-0.1f, 0.07f, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame4 = (Animation_Frame){
      0.75f,
      (Transform){
        V3(-0.1f, 0.07f, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    Animation_Frame frame5 = (Animation_Frame){
      0.85f,
      (Transform){
        V3(0.07f, 0.03f, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    Animation_Frame frame6 = (Animation_Frame){
      0.95f,
      (Transform){
        V3(0.07f, 0.03f, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    sb_push(a.frames, frame0);
    sb_push(a.frames, frame1);
    sb_push(a.frames, frame2);
    sb_push(a.frames, frame3);
    sb_push(a.frames, frame4);
    sb_push(a.frames, frame5);
    sb_push(a.frames, frame6);
    
    
    frame0.position = 1.0f;
    sb_push(a.frames, frame0);
    a.frame_count = sb_count(a.frames);
    a.speed = 0.1f;
    
    pack->animations[Robot_Animation_IDLE] = a;
  }
}


Animation_Instance *inst = &state->robot_anim;
inst->weights = arena_push_array(&state->arena, f32, Robot_Animation_COUNT);
inst->positions = arena_push_array(&state->arena, f32, Robot_Animation_COUNT);
