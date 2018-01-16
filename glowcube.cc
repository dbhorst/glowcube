// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how to use the library.
// For more examples, look at demo-main.cc
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"

#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>

using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
    interrupt_received = true;
}

// Conway's game of life
// Contributed by: Vliedel
class GameLife : public ThreadedCanvasManipulator {
public:
  GameLife(Canvas *m, int delay_ms=500, bool torus=true)
    : ThreadedCanvasManipulator(m), delay_ms_(delay_ms), torus_(torus) {
    width_ = canvas()->width();
    height_ = canvas()->height();

    // Allocate memory
    values_ = new int*[width_];
    for (int x=0; x<width_; ++x) {
      values_[x] = new int[height_];
    }
    newValues_ = new int*[width_];
    for (int x=0; x<width_; ++x) {
      newValues_[x] = new int[height_];
    }

    // Init values randomly
    srand(time(NULL));
    for (int x=0; x<width_; ++x) {
      for (int y=0; y<height_; ++y) {
        values_[x][y]=rand()%2;
      }
    }
    r_ = 238;
    g_ = 77;
    b_ = 46;
  }

  ~GameLife() {
    for (int x=0; x<width_; ++x) {
      delete [] values_[x];
    }
    delete [] values_;
    for (int x=0; x<width_; ++x) {
      delete [] newValues_[x];
    }
    delete [] newValues_;
  }

  void Run() {
    while (running() && !interrupt_received) {

      updateValues();

      for (int x=0; x<width_; ++x) {
        for (int y=0; y<height_; ++y) {
          if (values_[x][y])
            canvas()->SetPixel(x, y, r_, g_, b_);
          else
            canvas()->SetPixel(x, y, 0, 0, 0);
        }
      }
      usleep(delay_ms_ * 1000); // ms
    }
  }

private:
  int numAliveNeighbours(int x, int y) {
    int num=0;
    if (torus_) {
      // Edges are connected (torus)
      num += values_[(x-1+width_)%width_][(y-1+height_)%height_];
      num += values_[(x-1+width_)%width_][y                    ];
      num += values_[(x-1+width_)%width_][(y+1        )%height_];
      num += values_[(x+1       )%width_][(y-1+height_)%height_];
      num += values_[(x+1       )%width_][y                    ];
      num += values_[(x+1       )%width_][(y+1        )%height_];
      num += values_[x                  ][(y-1+height_)%height_];
      num += values_[x                  ][(y+1        )%height_];
    }
    else {
      // Edges are not connected (no torus)
      if (x>0) {
        if (y>0)
          num += values_[x-1][y-1];
        if (y<height_-1)
          num += values_[x-1][y+1];
        num += values_[x-1][y];
      }
      if (x<width_-1) {
        if (y>0)
          num += values_[x+1][y-1];
        if (y<31)
          num += values_[x+1][y+1];
        num += values_[x+1][y];
      }
      if (y>0)
        num += values_[x][y-1];
      if (y<height_-1)
        num += values_[x][y+1];
    }
    return num;
  }

  void updateValues() {
    // Copy values to newValues
    for (int x=0; x<width_; ++x) {
      for (int y=0; y<height_; ++y) {
        newValues_[x][y] = values_[x][y];
      }
    }
    // update newValues based on values
    for (int x=0; x<width_; ++x) {
      for (int y=0; y<height_; ++y) {
        int num = numAliveNeighbours(x,y);
        if (values_[x][y]) {
          // cell is alive
          if (num < 2 || num > 3)
            newValues_[x][y] = 0;
        }
        else {
          // cell is dead
          if (num == 3)
            newValues_[x][y] = 1;
        }
      }
    }
    // copy newValues to values
    for (int x=0; x<width_; ++x) {
      for (int y=0; y<height_; ++y) {
        values_[x][y] = newValues_[x][y];
      }
    }
  }

  int** values_;
  int** newValues_;
  int delay_ms_;
  int r_;
  int g_;
  int b_;
  int width_;
  int height_;
  bool torus_;
};

int main(int argc, char *argv[]) {
    RGBMatrix::Options defaults;
    defaults.hardware_mapping = "regular";  // or e.g. "adafruit-hat"
    defaults.rows = 64;
    defaults.chain_length = 12;
    defaults.parallel = 1;
    defaults.show_refresh_rate = false;
    Canvas *canvas = rgb_matrix::CreateMatrixFromFlags(&argc, &argv, &defaults);
    if (canvas == NULL)
        return 1;

    // It is always good to set up a signal handler to cleanly exit when we
    // receive a CTRL-C for instance. The DrawOnCanvas() routine is looking
    // for that.
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    int scroll_ms = 30;

    ThreadedCanvasManipulator *image_gen = new GameLife(canvas, scroll_ms);
    image_gen->Start();

    // Now, the image generation runs in the background. We can do arbitrary
    // things here in parallel. In this demo, we're essentially just
    // waiting for one of the conditions to exit.
    int runtime_seconds = -1;
    if (runtime_seconds > 0) {
      sleep(runtime_seconds);
    } else {
      // The
      printf("Press <CTRL-C> to exit and reset LEDs\n");
      while (!interrupt_received) {
        sleep(1); // Time doesn't really matter. The syscall will be interrupted.
      }
    }

    // Animation finished. Shut down the RGB matrix.
    canvas->Clear();
    delete image_gen;
    delete canvas;

    rintf("\%s. Exiting.\n",
         interrupt_received ? "Received CTRL-C" : "Timeout reached");

    return 0;
}