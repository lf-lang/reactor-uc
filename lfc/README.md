# LFC Fork
This is a very limited quick-and-dirty fork of Lingua Franca. I have deleted as
many source files as I could in order to get a minimal and fast lfc that can
code-generate for `reactor-uc`. Since this is a subdirectory within reactor-uc
we are not copying reactor-uc into the src-gen directories but rather expecting
an environment variable REACTOR_UC_PATH to be defined.

The goal is to eventually merge this upstream.
