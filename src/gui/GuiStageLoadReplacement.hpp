#pragma once

#include "../GkGuiOverlay.hpp"

#ifdef GKMOTION_GUI_ACTIVE
namespace GakuMotion
{
	// Developer-only selection for a future native Photography load. It may
	// be edited with developer mode enabled while general injection is denied.
	inline void DrawStageLoadReplacementControl(const GuiContext& ctx)
	{
		ImGui::PushID("stage_load_replacement");
		ImGui::TextUnformatted("摄影舞台替换（实验，载入时生效）");
		ImGui::TextWrapped("%zu 个固定场景入口；新增场景需游戏内验收。[舞台] 页可选择程序化灯光或原生歌曲时间轴；原生歌曲模式优先加载歌曲的配套舞台。", stage_load::stages.size());
		const auto* current = stage_load::FindStage(ctx.stageSettings.loadReplacementKey);
		ImGui::BeginDisabled(!ctx.fnSetStageLoadReplacement);
		if (ImGui::BeginCombo("下次摄影背景", current ? current->name : "原生摄影背景", ImGuiComboFlags_HeightLarge))
		{
			if (ImGui::Selectable("原生摄影背景", !current) && ctx.fnSetStageLoadReplacement)
				ctx.fnSetStageLoadReplacement(ctx.self, "");
			for (const auto& stage : stage_load::stages)
			{
				const bool selected = current == &stage;
				if (ImGui::Selectable(stage.name, selected) && ctx.fnSetStageLoadReplacement)
					ctx.fnSetStageLoadReplacement(ctx.self, stage.key);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		ImGui::TextWrapped("选择会自动保存，离开当前页面后保留，下次进入偶像摄影时生效。已在摄影中时，可使用游戏原生功能切换背景触发载入。");
		ImGui::TextWrapped("恢复原生：选择“原生摄影背景”，然后重新进入摄影或原生切换背景。当前场景不会立即改变。");
		if (!ctx.stageLoadNotice.empty())
			ImGui::TextWrapped("%s", ctx.stageLoadNotice.c_str());
		ImGui::PopID();
	}
}
#endif
