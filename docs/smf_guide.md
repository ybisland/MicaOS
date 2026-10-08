# State Machine Framework

> The State Machine Framework(SMF) is port from zephyr project.


## Concepts

Terms:

- State machine: 

  A model of behavior that has a set of states and transitions between them. Events or conditions cause the system to move from one state to another.

- Plain state machine: 

  A flat state machine where all states are at the same level. States do not contain other states.

  ```
  IDLE → RUNNING → STOPPED
  ```

- HSM (Hierarchical State Machine): 

  A state machine in which states can contain other states. This allows states to share common behavior and makes complex behavior easier to organize.

  ```
    CONNECTED
    ├── IDLE
    └── TRANSFERRING
  ```

- Leaf state: 
  
  A state that has no child states, such as `IDLE` or `TRANSFERRING`.

- Parent state: 
  
  A state that contains one or more child states, such as `CONNECTED`. It is also called a composite state or superstate.

HSM description:

A leaf state is the most **specific active state** and represents the system’s **concrete current condition**.

A parent state represents a **broader context** and can **provide behavior shared** by its child states.

For example, there are 3 states to represent a device network state.
```
DISCONNECTED

CONNECTED
├── IDLE
└── TRANSFERRING
```
Both the 2 child state `IDLE` and `TRANSFERRING` may receive a `DISCONNECT` event, so the event can be propagated to parent state `CONNECTED` to handle.

In general, a parent state is used to heanle the shared action of its child states and the active state should only be leaf state. A parent should not be the current active state. So the target state of transition should be a leaf state not a parent state.

However, if a parent state defines an initial transition to a child state, state machine is allowed to transition to a parent state. A well-formed HSM should have initial transitions defined for all parent states.

**User should notice that transition to a parent state without initial transition is not allowed.**

## State Creation

A state is represented by three functions, where one function implements the Entry actions, another function implements the Run actions, and the last function implements the Exit actions. 

The prototype for the entry and exit functions are as follows: `void funct(void *obj)`, and the prototype for the run action is `smf_state_result_t funct(void *obj)` where the obj parameter is a user defined structure that has the state machine context, `smf_ctx`, as its first member. For example:
```c
struct user_object {
   struct smf_ctx ctx;
   /* All User Defined Data Follows */
};
```

The `smf_ctx` member must be first because the state machine framework’s functions casts the user defined object to the `smf_ctx` type with the `SMF_CTX` macro.

For example instead of doing this `(smf_ctx_t *)&user_obj`, you could use `SMF_CTX(&user_obj)`.

By default, a state can have no ancestor states, resulting in a flat state machine. But to enable the creation of a hierarchical state machine, the `CONFIG_SMF_ANCESTOR_SUPPORT` option must be enabled.

The return value of the run action, `smf_state_result_t` determines if the state machine propagates the event to parent run actions (`SMF_EVENT_PROPAGATE`) or if the event was handled by the run action (`SMF_EVENT_HANDLED`). Flat state machines do not have parent actions, so the return code is ignored; returning `SMF_EVENT_HANDLED` is recommended.

Calling `smf_change_state()` prevents calling parent run actions, even if `SMF_EVENT_PROPAGATE` is returned.

The following macro can be used for easy state creation:

- `SMF_CREATE_STATE` Create a state

## State Machine Creation

A state machine is created by defining a table of states that’s indexed by an enum. For example, the following creates three flat states:

```c
enum demo_state { S0, S1, S2 };

const struct smf_state demo_states[] = {
   [S0] = SMF_CREATE_STATE(s0_entry, s0_run, s0_exit, NULL, NULL),
   [S1] = SMF_CREATE_STATE(s1_entry, s1_run, s1_exit, NULL, NULL),
   [S2] = SMF_CREATE_STATE(s2_entry, s2_run, s2_exit, NULL, NULL)
};
```

And this example creates three hierarchical states:

```c
enum demo_state { S0, S1, S2 };

const struct smf_state demo_states[] = {
   [S0] = SMF_CREATE_STATE(s0_entry, s0_run, s0_exit, parent_s0, NULL),
   [S1] = SMF_CREATE_STATE(s1_entry, s1_run, s1_exit, parent_s12, NULL),
   [S2] = SMF_CREATE_STATE(s2_entry, s2_run, s2_exit, parent_s12, NULL)
};
```

This example creates three hierarchical states with an initial transition from parent state S0 to child state S2:


```c
enum demo_state { S0, S1, S2 };

/* Forward declaration of state table */
const struct smf_state demo_states[];

const struct smf_state demo_states[] = {
   [S0] = SMF_CREATE_STATE(s0_entry, s0_run, s0_exit, NULL, demo_states[S2]),
   [S1] = SMF_CREATE_STATE(s1_entry, s1_run, s1_exit, demo_states[S0], NULL),
   [S2] = SMF_CREATE_STATE(s2_entry, s2_run, s2_exit, demo_states[S0], NULL)
};
```

## State Initialization

Call `smf_set_initial()` to set the initial state. Its entry action(and its parent ectry action) will be excute synchronously.

> A parent state without initial transition is not allowed to be passed to `smf_set_initial()`.

```
      │smf_set_initial()               
      │                                
┌─────┼────┐                           
│S0   │    │                           
│ ┌───▼──┐ │                           
│ │S01   │ │        Excute Flow:                     
│ │      │ │          1. S0 Entry                    
│ └──────┘ │          2. S01 Entry                   
└──────────┘                           
```

## State Transition
#
Call `smf_change_state()` to transition from one state to another. The exit and entry will be excute synchronously.

> A parent state without initial transition is not allowed to be passed to `smf_chagne_state()`.

> `smf_set_state` should only be called from the `Entry` or `Run` function. Calling from `Exit` functions is not allowed.

```
 ┌──────┐                     ┌──────┐ 
 │S0    │                     │S1    │ 
 │┌────┐│                     │┌────┐│ 
 ││S01 ││ smf_change_state()  ││S11 ││ 
 ││   ─┼┼─────────────────────┼┼►   ││ 
 │└────┘│                     │└────┘│ 
 └──────┘                     └──────┘ 
    Excute Flow:                       
      1. S01 Exit                      
      2. S0  Exit                      
      3. S1  Entry                     
      4. S11 Entry                     
```

> smf ctx does not record the current state and previous state, you can record the state in `user_object` to check the transition direction in the entry action.
  ```c
struct user_object {
   struct smf_ctx ctx;
   /* All User Defined Data Follows */
   state_t last_state;
};

static void entry(struct smf_ctx *ctx) {
    switch (ctx->last_state) {
      // ...
    }
}
  ```

## State Machine Execution

To run the state machine, the `smf_run_state()` function should be called in some application dependent way. An application should cease calling `smf_run_state()` if it returns a non-zero value, which means the state machine terminated.

The `smf_run_state()` runs one iteration of a state machine, including parent states if the child state returns `SMF_EVENT_PROPAGATE`.

## State Machine Termination

To terminate the state machine, the `smf_set_terminate()` function should be called. It can be called from the entry, run, or exit actions. The function takes a non-zero user defined value that will be returned by the `smf_run_state()` function.

## Retrieving the Current State

Use `smf_get_current_leaf_state()` to retrieve the current leaf state.

Use `smf_get_current_executing_state()` to retrieve the state whose entry, run, or exit action is currently being executed.

## State Machine Examples

Please see zephyr example:

https://docs.zephyrproject.org/latest/services/smf/index.html#state-machine-examples

## Test Instrumentation

The SMF provides optional instrumentation hooks for observing state machine behavior during testing. To enable them, set `CONFIG_SMF_INSTRUMENTATION`.

When enabled, three hook callbacks can be registered on a state machine context via `smf_set_hooks()`:

- on_action — called before each entry, run, or exit action executes.

- on_transition — called after the current state pointer is updated but before the new state’s entry actions execute.

- on_error — called when an invalid operation is detected (e.g. a NULL transition target or a transition attempted from an exit action).

> `smf_set_hooks()` must be called after `smf_set_initial()`, because `smf_set_initial()` resets the hooks pointer to `NULL`. As a consequence, entry actions executed during `smf_set_initial()` (i.e. the initial state’s entry actions and those of its ancestors) will not be captured by the hooks.

```c
#include <zephyr/smf.h>

static void on_action(struct smf_ctx *ctx,
                      const struct smf_state *state,
                      smf_action_type action_type)
{
        /* Log or record the action */
}

static void on_transition(struct smf_ctx *ctx,
                          const struct smf_state *source,
                          const struct smf_state *dest)
{
        /* Log or record the transition */
}

static const struct smf_hooks hooks = {
        .on_action = on_action,
        .on_transition = on_transition,
        /* .on_error = NULL — any member may be NULL */
};

void test_example(void)
{
        struct s_object s_obj;

        /* Set the initial state first */
        smf_set_initial(SMF_CTX(&s_obj), &demo_states[S0]);

        /* Install hooks after init — initial entry actions are not captured */
        smf_set_hooks(SMF_CTX(&s_obj), &hooks);

        /* Run the state machine — hooks fire on every action and transition */
        while (!smf_run_state(SMF_CTX(&s_obj))) {
            /* ... */
        }
}
```

> When CONFIG_SMF_INSTRUMENTATION is not set, all instrumentation code is compiled out and there is zero runtime overhead.