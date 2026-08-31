name: Bug Report (Beta)
about: Report a bug encountered while testing the Entergram Emulator
title: "[BUG] <concise description>"
labels: ["bug", "beta-testing"]
body:
  - type: dropdown
    id: game
    attributes:
      label: Game
      description: Which game are you playing?
      options:
        - Umineko When They Cry (Switch)
        - Higurashi When They Cry (Switch)
        - Other
    validations:
      required: true
  - type: textarea
    id: platform
    attributes:
      label: Platform
      description: Operating system, version, and architecture (e.g. Windows 11 22H2 x64, macOS 14.0 ARM, Ubuntu 22.04 x64)
      placeholder: "e.g. Windows 11 22H2 x64"
    validations:
      required: true
  - type: textarea
    id: description
    attributes:
      label: Description
      description: A clear description of the bug
    validations:
      required: true
  - type: textarea
    id: steps
    attributes:
      label: Steps to reproduce
      description: List the steps that lead to the bug
    validations:
      required: true
  - type: textarea
    id: expected
    attributes:
      label: Expected behavior
      description: What should have happened?
    validations:
      required: true
  - type: textarea
    id: actual
    attributes:
      label: Actual behavior
      description: What actually happened? Include any error messages.
    validations:
      required: true
  - type: textarea
    id: logs
    attributes:
      label: Logs/Screenshots
      description: Any relevant logs, error messages, or screenshots
  - type: input
    id: rom
    attributes:
      label: ROM version
      description: e.g. Umineko Switch v1.0.0, Higurashi PS3 version, etc.
    validations:
      required: false
