all : one two three

DEPENDENT_C=c

one: a b $(DEPENDENT_C)
    @echo one
two:; @echo two
three : "a;b" ;@echo three; @echo end of three

a :
b :
c :
"a;b":
    @echo X

DIRECTORYNAME=.
$(DIRECTORYNAME):
    @echo directory $(DIRECTORYNAME) doesn't exist. That's strange.
"$(DIRECTORYNAME)":
    @echo directory "$(DIRECTORYNAME)" doesn't exist. That's strange.

DIRECTORYNAME=..
$(DIRECTORYNAME):
    @echo directory $(DIRECTORYNAME) doesn't exist. That's strange.
"$(DIRECTORYNAME)":
    @echo directory "$(DIRECTORYNAME)" doesn't exist. That's strange.
