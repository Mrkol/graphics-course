#pragma once

//Virtual methods may affect performance. //TODO: Concepts + template magic;
class IGuiConfigurable {
  public:
    virtual void drawGui() {} 
};
