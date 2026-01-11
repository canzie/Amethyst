Retained mode

- backend agnostic
- need to define what the core does and what the backend implements
- what exact data is goes to the backed?
- the backend output is a set of command buffers that can be commited


# Elements

## General Containers
These objects are primarily used for wrapping other objects, however this is not an exclusive feature Everything can be a container, its just that objects like buttons Will have other features like text

### Frame / Div
### ScrollingFrame
### GridFrame
Similair to a frame but will, by default, contain a grid like pattern, which is then configurable, not to be confused with a grid layout.
More like the background of a plot
### Tables
#### Table Row
#### Table Header
#### Table Data

## GuiButtons
### TextButton
### ImageButton

## GuiLabels
### TextLabel
### ImageLabel

## Primitives
### Line
### Rectangle
### Triangle
### Circle
### Point

## UI Components
### ListLayout
### GridLayout
### 

## Widgets
### Plot
For allowing manual edit of the spline points, instead of pure data, tldr it is interactable
### Graph
For displaying a large amount of values, read only


# Intended Workflow

auto window = Amethyst::Frame();
auto button = Amethyst::TextButton();
button->parent = window;
button->text = "bla";






