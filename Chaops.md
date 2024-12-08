## Chaops

![Chaops](images/chaops.png)

The name "Chaops" is an abbreviation of "chaos operators". Chaops is a left-expander module that provides extra functionality to the chaos modules [Frolic](Frolic.md), [Glee](Glee.md), and [Lark](Lark.md). When you place Chaops to the immediate left of a Frolic, Glee, or Lark chaos module, Chaops allows you to access additional functionality not available from outside that chaos module.

### Memory

The section labeled MEMORY allows you to select one of 16 memory cells with addresses numbered 0..15.

Immediately below the MEMORY label, from left to right, are a CV input port, a small attenuverter knob, and a large manual selector knob. Turning the selector knob changes the active memory cell address, as reflected in the LED display.

You can automate selecting a memory cell by connecting an input cable to the memory CV input port and setting the attenuverter to a nonzero value. If you turn the attenuverter knob all the way clockwise to 100%, then each half volt (0.5&nbsp;V) increment of input voltage will increase the memory address by 1. Memory addresses are not clamped; instead, they wrap around. So by trying to go beyond address 15, you wrap back around to 0. Likewise, going below 0 will wrap around to 15.

Below the memory cell LED display are two buttons. The one on the left is labeled S, and the one on the right is labeled R. The S button stands for "store", and when pressed, causes the current state of the chaos module to the right to be stored inside the selected memory cell.

When you press the R button, which stands for "recall", the state of the chaos module is restored to the saved state from the selected memory cell.

Beneath the R and S buttons are corresponding trigger inputs that allow you to automate storing and recalling chaos states.

### Morph

### Freeze
