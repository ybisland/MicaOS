/*
 * Copyright 2021 The Chromium OS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * @brief State Machine Framework header file
 */

#ifndef SMF_H_
#define SMF_H_

#include <stdint.h>

/* clang-format off */
#if CONFIG_SMF_ANCESTOR_SUPPORT == 1
/**
 * @brief Macro to create a hierarchical state with initial transitions.
 *
 * @param _entry   State entry function or NULL
 * @param _run     State run function or NULL
 * @param _exit    State exit function or NULL
 * @param _parent  State parent object or NULL
 * @param _initial State initial transition object or NULL
 */
#define SMF_CREATE_STATE(_entry, _run, _exit, _parent, _initial)    \
{                                                                   \
    .entry   = _entry,                                              \
    .run     = _run,                                                \
    .exit    = _exit,                                               \
    .parent  = _parent,                                             \
    .initial = _initial,                                            \
}
#else
/**
 * @brief Macro to create a hierarchical state with initial transitions.
 *
 * @param _entry   State entry function or NULL
 * @param _run     State run function or NULL
 * @param _exit    State exit function or NULL
 */
#define SMF_CREATE_STATE(_entry, _run, _exit)                      \
{                                                                  \
    .entry   = _entry,                                             \
    .run     = _run,                                               \
    .exit    = _exit,                                              \
}
#endif
/* clang-format on */

/**
 * @brief Macro to cast user defined object to state machine context.
 *
 * @param o A pointer to the user defined object
 */
#define SMF_CTX(o) ((smf_ctx_t *)o)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief enum for the return value of a state_execution_pf function
 */
typedef enum smf_state_result {
    SMF_EVENT_HANDLED,   // event has been handled
    SMF_EVENT_PROPAGATE, // event should be handled by parent state
} smf_state_result_t;

/**
 * @brief Enum identifying which action type is being executed.
 * @note This is used for instrumentation purposes.
 */
typedef enum smf_action_type {
    SMF_ACTION_ENTRY, /**< Entry action */
    SMF_ACTION_RUN,   /**< Run action */
    SMF_ACTION_EXIT,  /**< Exit action */
} smf_action_type_t;

/** Error codes reported via the instrumentation error hook */
#define SMF_ERR_NULL_TRANSITION    1 /**< new_state is NULL in smf_change_state */
#define SMF_ERR_TRANSITION_IN_EXIT 2 /**< smf_change_state called in exit action */


/**
 * @brief Function pointer that implements a entry and exit actions of a state
 *
 * @param obj pointer user defined object
 */
typedef void (*state_method_pf)(void *obj);

/**
 * @brief Function pointer that implements a the run action of a state
 *
 * @param obj pointer user defined object
 * @return If the event should be propagated to parent states or not
 *         (Ignored when CONFIG_SMF_ANCESTOR_SUPPORT not defined)
 */
typedef smf_state_result_t (*state_execution_pf)(void *obj);

/** General state that can be used in multiple state machines. */
typedef struct smf_state {
    /** Optional method that will be run when this state is entered */
    const state_method_pf entry;

    /** Optional method that will be run repeatedly during state machine loop */
    const state_execution_pf run;

    /** Optional method that will be run when this state exists */
    const state_method_pf exit;

#if CONFIG_SMF_ANCESTOR_SUPPORT == 1
    /**
     * Optional parent state that contains common entry/run/exit
     *	implementation among various child states.
     *	entry: Parent function executes BEFORE child function.
     *	run:   Parent function executes AFTER child function.
     *	exit:  Parent function executes AFTER child function.
     *
     *	Note: When transitioning between two child states with a shared
     *      parent,	that parent's exit and entry functions do not execute.
     */
    const smf_state_t *parent;

    /** Optional initial transition state. NULL for leaf states */
    const smf_state_t *initial;
#endif /* CONFIG_SMF_ANCESTOR_SUPPORT */

} smf_state_t;

/** Defines the current context of the state machine. */
typedef struct smf_ctx {
    /** Current state the state machine is executing. */
    const smf_state_t *current;
    /** Previous state the state machine executed */
    const smf_state_t *previous;

#if CONFIG_SMF_ANCESTOR_SUPPORT == 1
    /** Currently executing state (which may be a parent) */
    const smf_state_t *executing;
#endif /* CONFIG_SMF_ANCESTOR_SUPPORT */

    /**
     * This value is set by the set_terminate function and
     * should terminate the state machine when its set to a
     * value other than zero when it's returned by the
     * run_state function.
     */
    int32_t terminate_val;
    /**
     * The state machine casts this to a "struct internal_ctx" and it's
     * used to track state machine context
     */
    uint32_t internal;

#if CONFIG_SMF_INSTRUMENTATION == 1
    /** Optional instrumentation hooks for testing and debugging */
    const struct smf_hooks *hooks;
#endif /* CONFIG_SMF_INSTRUMENTATION */

} smf_ctx_t;

/**
 * @brief Initializes the state machine and sets its initial state. The entry
 *        of initial state will be run synchronously.
 *
 * @param ctx        State machine context
 * @param init_state Initial state the state machine starts in.
 */
void smf_set_initial(smf_ctx_t *ctx, const smf_state_t *init_state);

/**
 * @brief Changes a state machines state. Excutes exit action of previous state
 *        and entry action of the target state. For HSMs, the entry and exit 
 *        actions of the Least Common Ancestor will not be run.
 *
 * @param ctx       State machine context
 * @param new_state State to transition to (NULL is invalid)
 */
void smf_change_state(smf_ctx_t *ctx, const smf_state_t *new_state);

/**
 * @brief Terminate a state machine
 *
 * @param ctx  State machine context
 * @param val  Non-Zero termination value that's returned by the smf_run_state
 *             function.
 */
void smf_set_terminate(smf_ctx_t *ctx, int32_t val);

/**
 * @brief Runs one iteration of a state machine (including any parent states)
 *
 * @param ctx  State machine context
 * @return	   A non-zero value should terminate the state machine. This
 *			   non-zero value could represent a terminal state being reached
 *			   or the detection of an error that should result in the
 *			   termination of the state machine.
 */
int32_t smf_run_state(smf_ctx_t *ctx);

/**
 * @brief Get the current leaf state.
 *
 * @note This may be a PARENT state if the HSM is malformed
 *		 (i.e. the initial transitions are not set up correctly).
 *
 * @param ctx State machine context
 * @return    The current leaf state.
 */
static inline const smf_state_t *smf_get_current_leaf_state(const smf_ctx_t *const ctx)
{
    return ctx->current;
}

/**
 * @brief Get the state that is currently executing. This may be a parent state.
 *
 * @param ctx State machine context
 * @return    The state that is currently executing.
 */
static inline const smf_state_t *smf_get_current_executing_state(const smf_ctx_t *const ctx)
{
#if CONFIG_SMF_ANCESTOR_SUPPORT == 1
    return ctx->executing;
#else
    return ctx->current;
#endif /* CONFIG_SMF_ANCESTOR_SUPPORT */
}

#if CONFIG_SMF_INSTRUMENTATION == 1
/**
 * @brief Called after the current state pointer is updated, before entry
 *        actions of the new state execute.
 *
 * @param ctx    State machine context
 * @param source Previous state (before transition)
 * @param dest   New current state (after transition)
 */
typedef void (*smf_transition_hook)(smf_ctx_t *ctx, const smf_state_t *source,
                                    const smf_state_t *dest);

/**
 * @brief Called before a state action (entry/run/exit) is invoked.
 *
 * @param ctx         State machine context
 * @param state       The state whose action is about to execute
 * @param action_type Which action (entry, run, or exit)
 */
typedef void (*smf_action_hook)(smf_ctx_t *ctx, const smf_state_t *state,
                                smf_action_type_t action_type);

/**
 * @brief Called when an invalid operation is detected.
 *
 * @param ctx        State machine context
 * @param error_code One of SMF_ERR_* defines
 */
typedef void (*smf_error_hook)(smf_ctx_t *ctx, int error_code);

/**
 * @brief Collection of optional instrumentation hooks.
 *
 * Any member may be NULL to skip that notification.
 */
struct smf_hooks {
    smf_transition_hook on_transition; /**< Hook called on transition */
    smf_action_hook on_action;         /**< Hook called on entry/run/exit actions */
    smf_error_hook on_error;           /**< Hook called on error */
};

/**
 * @brief Set instrumentation hooks on a state machine context.
 *
 * Must be called **after** smf_set_initial(), because smf_set_initial()
 * resets the hooks pointer to NULL. Entry actions executed during
 * smf_set_initial() (the initial state and its ancestors) will not be
 * captured by these hooks.
 *
 * @param ctx   State machine context
 * @param hooks Pointer to a hooks struct, or NULL to disable hooks.
 *              The pointed-to struct must outlive the state machine.
 */
void smf_set_hooks(smf_ctx_t *ctx, const struct smf_hooks *hooks);
#endif /* CONFIG_SMF_INSTRUMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* SMF_H_ */