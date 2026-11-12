#pragma once

void quad_bridge_query_while_locked(bool active) {
    // Captured CP77 cycle: render owns the core calibration lock and queries
    // quad status; EndFrame owns state_mutex and calls back into calibration.
    // Bound the wait and release state_mutex before joining, so the old layer
    // fails this regression instead of hanging the entire test process.
    std::mutex calibration;
    std::atomic<bool> owns_calibration{}, query_finished{};
    CheekyQuadViewsV1 snapshot{};
    snapshot.version=CHEEKY_QUAD_VIEWS_VERSION;snapshot.size=sizeof(snapshot);
    uint32_t result{};
    std::unique_lock state_lock(state_mutex);
    std::thread render([&] {
        std::lock_guard core_lock(calibration);
        owns_calibration=true;
        result=CheekyQuad_GetViewsV1(&snapshot);
        query_finished=true;
    });
    while(!owns_calibration.load())std::this_thread::yield();
    const auto deadline=GetTickCount64()+250;
    while(!query_finished.load()&&GetTickCount64()<deadline)Sleep(1);
    const bool completed_without_state=query_finished.load();
    const bool callback_can_enter=calibration.try_lock();
    if(callback_can_enter)calibration.unlock();
    state_lock.unlock();render.join();
    check(completed_without_state&&callback_can_enter&&result==1&&bool(snapshot.active)==active,
        "Quad query must preserve active status and release the core lock while EndFrame owns state_mutex");
}
void quad_bridge_lock_validation() {
    quad_bridge_query_while_locked(false);
    std::cout<<"PASS quad bridge lock order: render/core and OpenXR/state cannot deadlock\n";
}
