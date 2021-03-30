# CNC Robot Control
Public version of the control software for my thesis "Control of a CNC-type robot for 3D mapping applications". 

Project was implemented using a Jetson TX2. However, in theory, the libraries should work on other embedded Linux platforms, with minimal changes in the code, particulary to the GPIO library.

In this repository the following libraries are available, and found under the core directory:
  - Time: Utilities for doing arithmetic operations with timespec structs, and creating delays.  
  - Task: Create and manage threads.
  - GPIO: Depends on libgpiod (see https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/). Pin mappings for the GPIO lines on the J21 header of the Jetson, and functions for controlling them. Wraps around some functions and structs of libgpiod with more familiar names. 
  - Stepper: Control stepper motors either individually or in group. Said stepper motors should be connected to a A4988 driver, but other drivers with EN, STEP and DIR lines should work.
  - Axis: Control axes. An axis is composed of one or more stepper motors, and is linked to a physical dimensions of the robot. Thus, axes are controlled based on a desired linear displacement and speed.

Under the control directory, the main control program is found, along with other components for reading configuration files and managing interprocess communication between the control process and the data adquisition processes (not made available through this repository).
