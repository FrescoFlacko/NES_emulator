import type { Plugin } from "@opencode-ai/plugin"

const DEFAULT_PLAN_PATH = "PLAN.md"

export const PlanRunner: Plugin = async ({ client, directory }) => {
  const planPath = process.env.OPENCODE_PLAN_PATH || DEFAULT_PLAN_PATH
  const fullPlanPath = planPath.startsWith("/") ? planPath : `${directory}/${planPath}`

  let pendingKick = false
  let planComplete = false

  return {
    event: async ({ event }) => {
      if (event.type !== "session.idle") return
      if (pendingKick || planComplete) return

      let planContent: string | null = null
      try {
        const result = await client.file.read({ filePath: fullPlanPath })
        planContent = result?.content ?? null
      } catch {
        return
      }

      if (!planContent || planContent.trim() === "") return

      pendingKick = true
      try {
        await client.message.create({
          content: `## Plan Continuation Check

Review the current plan and your progress:

<plan>
${planContent}
</plan>

**Instructions:**
1. Evaluate what phases/sections exist in this plan (you determine the structure)
2. Assess which phase you are currently on and whether it is complete
3. If the current phase is incomplete, continue working on it
4. If the current phase is complete, move to the next incomplete phase
5. If ALL phases are complete, respond with exactly: \`[PLAN_COMPLETE]\`

Do not ask for user input. Either continue working or declare the plan complete.`,
        })
      } finally {
        pendingKick = false
      }
    },

    "message.updated": async ({ message }) => {
      if (
        message.role === "assistant" &&
        typeof message.content === "string" &&
        message.content.includes("[PLAN_COMPLETE]")
      ) {
        planComplete = true
      }
    },
  }
}
