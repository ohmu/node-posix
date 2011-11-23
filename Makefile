SHELL := /bin/bash
node-command := xargs -n 1 -I file node file $(params)

default:
	@echo need a target name...

test: test-unit 

bench:
	@find benchmark -name "*-bench.js" | $(node-command)

build/default/binding.node:
	@node-waf configure build

test-unit:
	@find test/unit -name "test-*.js" | $(node-command)

.PHONY : test build/default/binding.node
