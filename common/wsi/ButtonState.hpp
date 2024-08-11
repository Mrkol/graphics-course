#pragma once


enum class ButtonState
{
  Low = 0,     //< Button not active this frame
  Rising = 1,  //< Button was pressed on this exact frame
  High = 2,    //< Button was pressed on one of the previous frames and is being held
  Falling = 3, //< Button was released on this frame
};

inline bool is_held_down(ButtonState state)
{
  return state == ButtonState::Rising || state == ButtonState::High;
}
