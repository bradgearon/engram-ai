# Distilled — Design Patterns (GoF + Enterprise)

## Intent

Patterns are named, reusable solutions to recurring problems in a context, together with the consequences of using them.

## High-leverage enterprise patterns (Fowler EAA catalog)

- Transaction Script vs Domain Model vs Table Module — choose by complexity of domain logic.
- Repository — collection-like access to aggregates; hides persistence.
- Unit of Work — tracks changes and writes them consistently.
- Service Layer — application boundary that coordinates domain objects.
- Data Mapper vs Active Record — how much persistence knowledge lives in the domain.

## Structural / behavioral reminders

- Strategy, State, Observer, Command, Decorator remain useful once the boundary is clear.
- Prefer composition and explicit interfaces over deep inheritance for most new work.
- A pattern applied without its context or consequences is usually noise.

## Training signal

Given a problem statement, name the pattern, the problem it solves here, and one consequence or alternative that was rejected.
