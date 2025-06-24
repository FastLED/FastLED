

#ifndef FRAME_BUFFER_H_
#define FRAME_BUFFER_H_

struct Color3i;

class FrameBufferBase {
 public:
  FrameBufferBase(Color3i* array, int n_pixels);
  virtual ~FrameBufferBase();

  void Set(int i, const Color3i& c);
  void Set(int i, int length, const Color3i& color);
  void FillColor(const Color3i& color);
  void ApplyBlendSubtract(const Color3i& color);
  void ApplyBlendAdd(const Color3i& color);
  void ApplyBlendMultiply(const Color3i& color);
  Color3i* GetIterator(int i);

  // Length in pixels.
  int length() const;

 protected:
  Color3i* color_array_;
  int n_color_array_;
};

class FrameBuffer : public FrameBufferBase {
 public:
  FrameBuffer(int n_pixels);
  virtual ~FrameBuffer();
};

#endif // FRAME_BUFFER_H_
