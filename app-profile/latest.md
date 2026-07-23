---
format: pipbong-app-profile
format_version: 5
profiling_depth: standard
app_version: 0.8.294
git_sha: 7e75ba0
session_start: 2026-07-24T03:37:25.194
session_end: 2026-07-24T03:55:56.858
end_reason: app_shutdown
feature_id: de22e11a-f05d-4a65-aa10-7880747ce941
feature_name: 올리폿
run_mode: Trigger
profile_name: LOL
workflow_block_count: 52
session_start_source: restore
events_recorded: 5861
events_dropped: 0
---

# App profile

## Session

| Field | Value |
| --- | --- |
| Profile | LOL |
| Feature | 올리폿 (`de22e11a-f05d-4a65-aa10-7880747ce941`) |
| Run mode | Trigger |
| Start source | restore |
| Workflow blocks | 52 |
| Started | 2026-07-24T03:37:25.194 |
| Ended | 2026-07-24T03:55:56.858 |
| End reason | app_shutdown |
| Events | 5861 recorded, 0 dropped |
| Counters | loops 2/2, blocks 2, clicks 0, ui_flush 0 |

## Metrics (milliseconds)

| Series | count | p50 | p95 | p99 | max |
| --- | ---: | ---: | ---: | ---: | ---: |
| loop_duration | 2 | 1424.584 | 1427.719 | 1427.998 | 1428.068 |
| loop_gap | 0 | n/a | n/a | n/a | 0.000 |
| ui_fast_repeat_flush | 0 | — | n/a | — | 0.000 |
| synthetic_mouse_click | 0 | — | n/a | — | 0.000 |
| synthetic_key | 0 | n/a | n/a | n/a | 0.000 |

## Spike counts

| Kind | threshold | count |
| --- | --- | ---: |
| loop_gap | >16 ms | 0 |
| loop_gap | >32 ms | 0 |
| loop_gap | >50 ms | 0 |
| loop_duration | >32 ms | 2 |
| ui_flush | >8 ms | 0 |
| synthetic_mouse_click | >12 ms | 0 |
| synthetic_key | >80 ms | 0 |

## Profile settings (session snapshot)

| Setting | Value |
| --- | --- |
| profile_id | 497ab714-9a83-4b14-ab8c-e9d90ba4a663 |
| profile_name | LOL |
| main_target | League of Legends (TM) Client |
| sub_target | League of Legends |
| auto_select_running | yes |
| pin_main_center | no |
| pin_sub_center | no |
| imagefind_capture | Hybrid |
| run_without_target | yes |
| linked_process | D:\Riot Games\League of Legends\Game\League of Legends.exe |
| sub_linked_process | D:\Riot Games\League of Legends\LeagueClientUx.exe |
| trigger_armed_count | 2 |

## Feature settings (session snapshot)

| Setting | Value |
| --- | --- |
| run_mode | Trigger |
| trigger_cooldown_ms | 30000 |
| trigger_background | no |
| capture_target_scope | SubOnly |
| require_scoped_target_foreground | yes |
## Workflow blocks (session snapshot)

| # | Type | Config |
| ---: | --- | --- |
| 1 | ImageFind | templates=1 poll=300ms thr=0.85 roi=1 |
| 2 | Wait | 2000 ms |
| 3 | Click | Fixed Left×1 @(285,223) |
| 4 | Click | Fixed Left×1 @(519,272) |
| 5 | Click | Fixed Left×1 @(519,324) |
| 6 | Click | Fixed Left×1 @(521,630) |
| 7 | Click | Fixed Left×1 @(869,820) |
| 8 | Click | Fixed Left×1 @(285,278) |
| 9 | Click | Fixed Left×1 @(519,272) |
| 10 | Click | Fixed Left×1 @(519,324) |
| 11 | Click | Fixed Left×1 @(521,630) |
| 12 | Click | Fixed Left×1 @(869,820) |
| 13 | Click | Fixed Left×1 @(281,322) |
| 14 | Click | Fixed Left×1 @(524,271) |
| 15 | Click | Fixed Left×1 @(521,327) |
| 16 | Click | Fixed Left×1 @(519,628) |
| 17 | Click | Fixed Left×1 @(867,819) |
| 18 | Click | Fixed Left×1 @(282,369) |
| 19 | Click | Fixed Left×1 @(523,272) |
| 20 | Click | Fixed Left×1 @(516,325) |
| 21 | Click | Fixed Left×1 @(522,625) |
| 22 | Click | Fixed Left×1 @(855,821) |
| 23 | Click | Fixed Left×1 @(276,425) |
| 24 | Click | Fixed Left×1 @(516,275) |
| 25 | Click | Fixed Left×1 @(516,326) |
| 26 | Click | Fixed Left×1 @(523,627) |
| 27 | Click | Fixed Left×1 @(853,819) |
| 28 | Click | Fixed Left×1 @(288,515) |
| 29 | Click | Fixed Left×1 @(522,275) |
| 30 | Click | Fixed Left×1 @(523,320) |
| 31 | Click | Fixed Left×1 @(522,627) |
| 32 | Click | Fixed Left×1 @(848,828) |
| 33 | Click | Fixed Left×1 @(285,567) |
| 34 | Click | Fixed Left×1 @(523,274) |
| 35 | Click | Fixed Left×1 @(524,334) |
| 36 | Click | Fixed Left×1 @(523,625) |
| 37 | Click | Fixed Left×1 @(847,828) |
| 38 | Click | Fixed Left×1 @(282,625) |
| 39 | Click | Fixed Left×1 @(517,277) |
| 40 | Click | Fixed Left×1 @(519,325) |
| 41 | Click | Fixed Left×1 @(519,629) |
| 42 | Click | Fixed Left×1 @(850,820) |
| 43 | Click | Fixed Left×1 @(281,672) |
| 44 | Click | Fixed Left×1 @(519,272) |
| 45 | Click | Fixed Left×1 @(521,323) |
| 46 | Click | Fixed Left×1 @(523,629) |
| 47 | Click | Fixed Left×1 @(855,818) |
| 48 | Click | Fixed Left×1 @(285,726) |
| 49 | Click | Fixed Left×1 @(521,272) |
| 50 | Click | Fixed Left×1 @(524,328) |
| 51 | Click | Fixed Left×1 @(523,627) |
| 52 | Click | Fixed Left×1 @(855,824) |

## Block execution (measured)

| # | Type | runs | ok | fail | total (ms) | avg (ms) | max (ms) |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | ImageFind | 2 | 0 | 2 | 2849.0 | 1424.5 | 1428.0 |

## Auto diagnosis

_Recorded automatically from the feature/workflow snapshot and measured events — no manual workflow description required._

**병목 후보 (측정 기준)**

- #1 ImageFind: 실패 2회 (최대 **1428.0 ms** 블록) — 템플릿 미매칭/감시 대기

**루프**: 2회 완료

**AI**: 위 스냅샷·측정·이벤트만으로 원인 분석하세요. 사용자에게 워크플로 구성을 다시 묻지 마세요.

## Top loop_gap spikes

| rel (ms) | gap (ms) | after_loop |
| ---: | ---: | ---: |
| — | — | — |

## Top ui_flush spikes

| rel (ms) | dur (ms) | detail |
| ---: | ---: | --- |
| — | — | — |

## Event series (milliseconds)

| event | count | p50 | p95 | max |
| --- | ---: | ---: | ---: | ---: |
| spike_loop_duration | 783 | 39.076 | 82.296 | 60844.450 |
| ui_fast_repeat_flush | 746 | 0.421 | 0.748 | 2.082 |
| spike_loop_gap | 733 | 110.669 | 5400.086 | 75696.213 |
| hotkey_hold_start_dispatch | 576 | 3.140 | 4.359 | 9.122 |
| hotkey_hold_end_dispatch | 576 | 1.171 | 9.701 | 639.773 |
| session_end | 565 | — | — | — |
| session_begin | 565 | — | — | — |
| WorkflowEditorPanel.refresh | 307 | 0.000 | 2.699 | 8.000 |
| refreshWorkflowEditor | 291 | 2.000 | 4.000 | 15.000 |
| loop_interval_sleep | 238 | 62.682 | 80.429 | 81.843 |
| imagefind_poll | 187 | 43.496 | 46.037 | 147.722 |
| synthetic_mouse_click | 53 | 40.107 | 52.543 | 53.621 |
| spike_synthetic_key | 30 | 336.622 | 782.382 | 954.999 |
| trigger_monitor_start | 20 | — | — | — |
| syncHotkeys | 17 | 0.000 | 0.000 | 0.000 |
| loadProjectFromFile | 16 | 2.000 | 18.750 | 33.000 |
| loadActiveProfile | 16 | 2.500 | 18.750 | 33.000 |
| profile_switch | 15 | 7.623 | 41.637 | 57.613 |
| switchToProfile.saveSettings | 15 | 0.000 | 0.000 | 0.000 |
| switchToProfile.maybeSave | 15 | 0.000 | 0.000 | 0.000 |
| stopAllSessionsForProfileSwitch | 15 | 0.000 | 0.000 | 0.000 |
| switchToProfile.stopSessions | 15 | 0.000 | 0.000 | 0.000 |
| switchToProfile.loadActiveProfile | 15 | 3.000 | 19.699 | 33.000 |
| switchToProfile | 15 | 7.000 | 40.899 | 57.000 |
| hotkey_trigger_dispatch | 13 | 0.011 | 0.029 | 0.037 |
| capture_imagefind | 7 | 3.981 | 4.383 | 4.491 |
| user_input_interrupt | 4 | — | — | — |
| trigger_action_start | 4 | — | — | — |
| trigger_cooldown_start | 4 | — | — | — |
| profile_package_seal | 2 | 12.375 | 23.316 | 24.532 |
| app_trace_begin | 1 | — | — | — |
| auto_save | 1 | 1.100 | 1.100 | 1.100 |
| app_trace_end | 1 | — | — | — |

## Trigger phase (aggregates)

| Phase | count |
| --- | ---: |
| monitor_start | 20 |
| action_start | 4 |
| cooldown_start | 4 |

## Foreground timeline (sample)

| rel (ms) | window title |
| ---: | --- |
| — | — |

## Analysis hints

- **Profile settings** / **Feature settings** / **Workflow blocks**: frozen at `session_begin` — do not ask the user to describe the workflow.
- **Auto diagnosis**: Korean summary from snapshot + measured `block_end` / events.
- **profiling_depth**: `standard` (spike-filtered ImageFind polls), `detailed` (match/miss + spikes), `ultra` (every poll).
- **user_physical_***: real keyboard/mouse during runs; **synthetic_***: PIPBONG `SendInput` / injected clicks and keys.
- **imagefind_poll**: per-poll capture/match timing (`cap_us`, `match_us`, confidence); correlate with `loop=` on events.
- **foreground_change**: top-level window title timeline (Detailed+).
- **trace.json**: Chrome Trace JSON beside `latest.md` for timeline viewers.
- **app_trace_begin** / **app_trace_end**: full app lifetime while profiling is enabled.
- **session_begin** / **session_end**: feature run window inside the app trace.
- **loop_gap**: `loop_end` → next `loop_begin` idle time (scheduling / UI-thread stalls).
- **profile_switch** / **load_profile** / **auto_save** / **hotkey_***: app-wide UI and I/O paths.
- **capture_imagefind** / **imagefind_poll**: screen capture and template matching on the worker thread.
- **synthetic_mouse_click** / **synthetic_key**: PIPBONG input injection on the worker thread.
- **AI entry point:** repo `app-profile/latest.md` only (single file, overwritten each flush).

## Event log

Microsecond TSV columns: `rel_us`, `thread`, `event`, `detail`, `dur_us` (blank when N/A).

```tsv
rel_us	thread	event	detail	dur_us
7	ui	app_trace_begin	version=0.8.294
400702	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
401060	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
401222	ui	refreshWorkflowEditor	elapsed_ms=0	0
401313	ui	syncHotkeys	elapsed_ms=0	0
401622	ui	loadProjectFromFile	elapsed_ms=1	1000
401663	ui	loadActiveProfile	elapsed_ms=1	1000
499226	ui	syncHotkeys	elapsed_ms=0	0
1937767	ui	switchToProfile.maybeSave	elapsed_ms=0	0
1937894	ui	switchToProfile.saveSettings	elapsed_ms=0	0
1938167	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
1938195	ui	switchToProfile.stopSessions	elapsed_ms=0	0
1939851	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1947563	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
1948114	ui	refreshWorkflowEditor	elapsed_ms=4	4000
1951519	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
1952627	ui	refreshWorkflowEditor	elapsed_ms=4	4000
1953304	ui	syncHotkeys	elapsed_ms=0	0
1953447	ui	loadProjectFromFile	elapsed_ms=14	14000
1953489	ui	loadActiveProfile	elapsed_ms=14	14000
1953501	ui	switchToProfile.loadActiveProfile	elapsed_ms=14	14000
1972188	ui	switchToProfile	elapsed_ms=34	34000
1972233	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	34791
1977414	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
1977944	ui	refreshWorkflowEditor	elapsed_ms=3	3000
1985972	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
1987328	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1988743	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1989689	ui	trigger_monitor_start	block=#1
1990151	ui	session_end	reason=preempted_by_new_session
1990247	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
1995506	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
1996178	ui	refreshWorkflowEditor	elapsed_ms=4	4000
1998531	ui	trigger_monitor_start	block=#1
5397881	ui	profile_package_seal	sync=no	219
6596747	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	9
6597231	ui	session_end	reason=preempted_by_new_session
6597289	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
6599214	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
6599835	ui	refreshWorkflowEditor	elapsed_ms=2	2000
6644829	ui	user_input_interrupt	feature=aad48461-acdc-4484-a316-0637c4b4b8f3 vk=0x20
6647330	worker	spike_loop_duration	iter=0	41412
6649549	ui	ui_fast_repeat_flush	pending_iters=1	2082
6649554	ui	session_end	finish success=no
7487269	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	7
7487558	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
7491885	worker	spike_loop_gap	after_loop=0 gap_us=844554	844554
7581848	worker	spike_loop_duration	iter=0	89961
7586455	ui	ui_fast_repeat_flush	pending_iters=1	425
7586637	ui	session_end	finish success=yes
9173475	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	9
9173969	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
9179287	worker	spike_loop_gap	after_loop=0 gap_us=1597436	1597436
9268608	worker	spike_loop_duration	iter=0	89316
9269252	ui	ui_fast_repeat_flush	pending_iters=1	447
9269434	ui	session_end	finish success=yes
10415339	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
10417829	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2977
10419626	worker	spike_loop_gap	after_loop=0 gap_us=1151016	1151016
10420591	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
10420990	ui	refreshWorkflowEditor	elapsed_ms=2	2000
10458161	worker	spike_loop_duration	iter=0	38530
10459261	ui	ui_fast_repeat_flush	pending_iters=1	597
10526552	worker	loop_interval_sleep	requested_ms=67	68256
10526563	worker	spike_loop_gap	after_loop=0 gap_us=68403	68403
10562570	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1217
10565261	worker	spike_loop_duration	iter=1 loop=1	38695
10565683	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	258
10565687	ui	session_end	finish success=yes loop=1
12855569	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
12858758	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3737
12860343	worker	spike_loop_gap	after_loop=1 gap_us=2295080	2295080
12860905	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
12861318	ui	refreshWorkflowEditor	elapsed_ms=1	1000
12898023	worker	spike_loop_duration	iter=0	37677
12899232	ui	ui_fast_repeat_flush	pending_iters=1	586
12958525	worker	loop_interval_sleep	requested_ms=59	60391
12958532	worker	spike_loop_gap	after_loop=0 gap_us=60509	60509
12995363	worker	spike_loop_duration	iter=1 loop=1	36827
12996182	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	583
13009968	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1238
13016251	ui	session_end	finish success=yes loop=1
13232309	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
13234752	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2915
13236889	worker	spike_loop_gap	after_loop=1 gap_us=241527	241527
13240863	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
13241365	ui	refreshWorkflowEditor	elapsed_ms=1	1000
13278327	worker	spike_loop_duration	iter=0	41433
13279876	ui	ui_fast_repeat_flush	pending_iters=1	398
13340963	worker	loop_interval_sleep	requested_ms=61	62542
13340970	worker	spike_loop_gap	after_loop=0 gap_us=62643	62643
13374357	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1283
13377455	worker	spike_loop_duration	iter=1 loop=1	36482
13379208	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	832
13379214	ui	session_end	finish success=yes loop=1
13528691	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
13531624	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3485
13533628	worker	spike_loop_gap	after_loop=1 gap_us=156172	156172
13533885	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
13534320	ui	refreshWorkflowEditor	elapsed_ms=2	2000
13572708	worker	spike_loop_duration	iter=0	39076
13573223	ui	ui_fast_repeat_flush	pending_iters=1	356
13617223	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1145
13623728	ui	session_end	finish success=yes
13689283	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
13692104	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3277
13693978	worker	spike_loop_gap	after_loop=0 gap_us=121268	121268
13736715	worker	spike_loop_duration	iter=0	42736
13737460	ui	ui_fast_repeat_flush	pending_iters=1	409
13775242	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1157
13778505	ui	session_end	finish success=yes
13842188	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
13845211	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3480
13847311	worker	spike_loop_gap	after_loop=0 gap_us=110593	110593
13888742	worker	spike_loop_duration	iter=0	41428
13889675	ui	ui_fast_repeat_flush	pending_iters=1	697
13941864	worker	loop_interval_sleep	requested_ms=52	53039
13941871	worker	spike_loop_gap	after_loop=0 gap_us=53128	53128
14245862	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	303991
14278588	worker	spike_synthetic_key	dur_us=336697 loop=1	336697
14278604	worker	spike_loop_duration	iter=1 loop=1	336732
14279109	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	416
14279141	ui	session_end	finish success=yes loop=1
14860545	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
14863321	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3367
14864795	worker	spike_loop_gap	after_loop=1 gap_us=586191	586191
14865330	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
14865719	ui	refreshWorkflowEditor	elapsed_ms=1	1000
14902586	worker	spike_loop_duration	iter=0	37785
14903142	ui	ui_fast_repeat_flush	pending_iters=1	404
14952071	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1304
14955122	ui	session_end	finish success=yes
15020044	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
15022257	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2687
15023910	worker	spike_loop_gap	after_loop=0 gap_us=121324	121324
15063388	worker	spike_loop_duration	iter=0	39474
15064097	ui	ui_fast_repeat_flush	pending_iters=1	439
15120824	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1453
15124824	ui	session_end	finish success=yes
15174916	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
15178045	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3598
15179698	worker	spike_loop_gap	after_loop=0 gap_us=116310	116310
15216112	worker	spike_loop_duration	iter=0	36406
15217182	ui	ui_fast_repeat_flush	pending_iters=1	476
15294981	worker	loop_interval_sleep	requested_ms=77	78768
15294987	worker	spike_loop_gap	after_loop=0 gap_us=78877	78877
15314729	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1245
15331875	worker	spike_loop_duration	iter=1 loop=1	36874
15333279	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	692
15333286	ui	session_end	finish success=yes loop=1
17248322	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
17251217	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3414
17253058	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
17253275	worker	spike_loop_gap	after_loop=1 gap_us=1921407	1921407
17253654	ui	refreshWorkflowEditor	elapsed_ms=1	1000
17290745	worker	spike_loop_duration	iter=0	37467
17291333	ui	ui_fast_repeat_flush	pending_iters=1	416
17352217	worker	loop_interval_sleep	requested_ms=60	61373
17352225	worker	spike_loop_gap	after_loop=0 gap_us=61479	61479
17688827	worker	spike_synthetic_key	dur_us=336580 loop=1	336580
17688843	worker	spike_loop_duration	iter=1 loop=1	336617
17689971	ui	session_end	finish success=yes loop=1
17694584	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	280871
18472519	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
18475450	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3453
18477201	worker	spike_loop_gap	after_loop=1 gap_us=788356	788356
18477557	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
18478004	ui	refreshWorkflowEditor	elapsed_ms=2	2000
18514758	worker	spike_loop_duration	iter=0	37551
18515429	ui	ui_fast_repeat_flush	pending_iters=1	510
18572531	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1071
18576212	ui	session_end	finish success=yes
18615391	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
18617988	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3042
18620723	worker	spike_loop_gap	after_loop=0 gap_us=105966	105966
18657826	worker	spike_loop_duration	iter=0	37099
18659171	ui	ui_fast_repeat_flush	pending_iters=1	627
18721035	worker	loop_interval_sleep	requested_ms=62	63110
18721042	worker	spike_loop_gap	after_loop=0 gap_us=63216	63216
18746879	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1146
18757272	worker	spike_loop_duration	iter=1 loop=1	36227
18757765	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	314
18757769	ui	session_end	finish success=yes loop=1
20338289	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
20340646	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2748
20342807	worker	spike_loop_gap	after_loop=1 gap_us=1585536	1585536
20343801	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
20344238	ui	refreshWorkflowEditor	elapsed_ms=2	2000
20382783	worker	spike_loop_duration	iter=0	39973
20387447	ui	ui_fast_repeat_flush	pending_iters=1	416
20447988	worker	loop_interval_sleep	requested_ms=64	65134
20447994	worker	spike_loop_gap	after_loop=0 gap_us=65212	65212
20485906	worker	spike_loop_duration	iter=1 loop=1	37909
20486475	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	297
20540979	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1119
20547323	ui	session_end	finish success=yes loop=1
20894738	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
20897264	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3188
20898727	worker	spike_loop_gap	after_loop=1 gap_us=412819	412819
20899189	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
20899579	ui	refreshWorkflowEditor	elapsed_ms=1	1000
20936430	worker	spike_loop_duration	iter=0	37700
20937066	ui	ui_fast_repeat_flush	pending_iters=1	402
21000797	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1163
21007692	ui	session_end	finish success=yes
21105522	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
21107806	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2729
21109438	worker	spike_loop_gap	after_loop=0 gap_us=173007	173007
21146406	worker	spike_loop_duration	iter=0	36964
21147187	ui	ui_fast_repeat_flush	pending_iters=1	465
21188471	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1032
21197747	ui	session_end	finish success=yes
21269963	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
21272824	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3417
21274330	worker	spike_loop_gap	after_loop=0 gap_us=127924	127924
21274978	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
21275455	ui	refreshWorkflowEditor	elapsed_ms=2	2000
21312224	worker	spike_loop_duration	iter=0	37890
21312964	ui	ui_fast_repeat_flush	pending_iters=1	446
21361209	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1415
21364182	ui	session_end	finish success=yes
21418191	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
21421425	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3718
21423047	worker	spike_loop_gap	after_loop=0 gap_us=110823	110823
21459715	worker	spike_loop_duration	iter=0	36664
21460383	ui	ui_fast_repeat_flush	pending_iters=1	452
21513147	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1319
21521826	ui	session_end	finish success=yes
21555307	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
21558154	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3324
21559863	worker	spike_loop_gap	after_loop=0 gap_us=100147	100147
21596669	worker	spike_loop_duration	iter=0	36798
21597183	ui	ui_fast_repeat_flush	pending_iters=1	353
21675011	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1231
21678244	ui	session_end	finish success=yes
22096167	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
22099325	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3710
22101525	worker	spike_loop_gap	after_loop=0 gap_us=504858	504858
22102210	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
22102590	ui	refreshWorkflowEditor	elapsed_ms=2	2000
22139473	worker	spike_loop_duration	iter=0	37944
22140183	ui	ui_fast_repeat_flush	pending_iters=1	527
22198138	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1071
22200736	ui	session_end	finish success=yes
23909571	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
23912397	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3385
23914080	worker	spike_loop_gap	after_loop=0 gap_us=1774607	1774607
23914951	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
23915339	ui	refreshWorkflowEditor	elapsed_ms=2	2000
23952411	worker	spike_loop_duration	iter=0	38327
23954044	ui	ui_fast_repeat_flush	pending_iters=1	756
24033991	worker	loop_interval_sleep	requested_ms=80	81425
24033997	worker	spike_loop_gap	after_loop=0 gap_us=81587	81587
24058802	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1202
24072320	worker	spike_loop_duration	iter=1 loop=1	38320
24072737	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	344
24072801	ui	session_end	finish success=yes loop=1
24169635	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
24173074	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	4238
24176076	worker	spike_loop_gap	after_loop=1 gap_us=103755	103755
24213304	worker	spike_loop_duration	iter=0	37222
24214035	ui	ui_fast_repeat_flush	pending_iters=1	490
24277662	worker	loop_interval_sleep	requested_ms=63	64279
24277671	worker	spike_loop_gap	after_loop=0 gap_us=64368	64368
24303771	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1912
24314254	worker	spike_loop_duration	iter=1 loop=1	36581
24314597	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	290
24314627	ui	session_end	finish success=yes loop=1
25846728	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
25849243	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3154
25850754	worker	spike_loop_gap	after_loop=1 gap_us=1536498	1536498
25851401	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
25851804	ui	refreshWorkflowEditor	elapsed_ms=2	2000
25888314	worker	spike_loop_duration	iter=0	37558
25888935	ui	ui_fast_repeat_flush	pending_iters=1	430
25951815	worker	loop_interval_sleep	requested_ms=62	63392
25951822	worker	spike_loop_gap	after_loop=0 gap_us=63507	63507
25954539	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	962
25989788	worker	spike_loop_duration	iter=1 loop=1	37963
25990553	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	689
25990724	ui	session_end	finish success=yes loop=1
26016392	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
26019797	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3907
26021384	worker	spike_loop_gap	after_loop=1 gap_us=31596	31596
26060979	worker	spike_loop_duration	iter=0	39591
26061549	ui	ui_fast_repeat_flush	pending_iters=1	400
26112828	worker	loop_interval_sleep	requested_ms=51	51757
26112834	worker	spike_loop_gap	after_loop=0 gap_us=51856	51856
26139547	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1100
26148938	worker	spike_loop_duration	iter=1 loop=1	36101
26149387	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	375
26149531	ui	session_end	finish success=yes loop=1
26180374	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
26182774	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2980
26184293	worker	spike_loop_gap	after_loop=1 gap_us=35355	35355
26223995	worker	spike_loop_duration	iter=0	39699
26224686	ui	ui_fast_repeat_flush	pending_iters=1	422
26299280	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1283
26305237	ui	session_end	finish success=yes
27139558	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
27142347	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3250
27143926	worker	spike_loop_gap	after_loop=0 gap_us=919930	919930
27186621	worker	spike_loop_duration	iter=0	42693
27187337	ui	ui_fast_repeat_flush	pending_iters=1	430
27242594	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1014
27247694	ui	session_end	finish success=yes
27299351	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
27301540	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3178
27303582	worker	spike_loop_gap	after_loop=0 gap_us=116957	116957
27340793	worker	spike_loop_duration	iter=0	37209
27341789	ui	ui_fast_repeat_flush	pending_iters=1	443
27415288	worker	loop_interval_sleep	requested_ms=73	74452
27415294	worker	spike_loop_gap	after_loop=0 gap_us=74501	74501
27428066	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1151
27451448	worker	spike_loop_duration	iter=1 loop=1	36150
27452304	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	357
27452309	ui	session_end	finish success=yes loop=1
28146771	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
28149077	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3504
28150747	worker	spike_loop_gap	after_loop=1 gap_us=699300	699300
28151158	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
28151603	ui	refreshWorkflowEditor	elapsed_ms=2	2000
28188690	worker	spike_loop_duration	iter=0	37937
28189515	ui	ui_fast_repeat_flush	pending_iters=1	634
28242097	worker	loop_interval_sleep	requested_ms=52	53312
28242107	worker	spike_loop_gap	after_loop=0 gap_us=53416	53416
28279941	worker	spike_loop_duration	iter=1 loop=1	37831
28280414	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	401
28304501	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1073
28310893	ui	session_end	finish success=yes loop=1
28482116	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
28486023	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	4380
28487919	worker	spike_loop_gap	after_loop=1 gap_us=207977	207977
28491425	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
28491822	ui	refreshWorkflowEditor	elapsed_ms=1	1000
28529316	worker	spike_loop_duration	iter=0	41391
28530018	ui	ui_fast_repeat_flush	pending_iters=1	464
28608305	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1166
28610613	ui	session_end	finish success=yes
28647580	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
28650053	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2958
28651922	worker	spike_loop_gap	after_loop=0 gap_us=122606	122606
28652237	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
28652707	ui	refreshWorkflowEditor	elapsed_ms=2	2000
28689549	worker	spike_loop_duration	iter=0	37622
28690127	ui	ui_fast_repeat_flush	pending_iters=1	412
28744917	worker	loop_interval_sleep	requested_ms=54	55271
28744926	worker	spike_loop_gap	after_loop=0 gap_us=55377	55377
28756865	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1260
28781171	worker	spike_loop_duration	iter=1 loop=1	36243
28781637	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	394
28781891	ui	session_end	finish success=yes loop=1
28821524	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
28824230	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3200
28825737	worker	spike_loop_gap	after_loop=1 gap_us=44565	44565
28864826	worker	spike_loop_duration	iter=0	39086
28865427	ui	ui_fast_repeat_flush	pending_iters=1	402
28941660	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1351
28944783	ui	session_end	finish success=yes
29528258	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
29530531	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2752
29532328	worker	spike_loop_gap	after_loop=0 gap_us=667501	667501
29532744	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
29533121	ui	refreshWorkflowEditor	elapsed_ms=2	2000
29569857	worker	spike_loop_duration	iter=0	37525
29570462	ui	ui_fast_repeat_flush	pending_iters=1	439
29609024	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1200
29611236	ui	session_end	finish success=yes
29680377	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
29683037	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3128
29684697	worker	spike_loop_gap	after_loop=0 gap_us=114837	114837
29724143	worker	spike_loop_duration	iter=0	39442
29724983	ui	ui_fast_repeat_flush	pending_iters=1	443
29775292	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	974
29775490	ui	session_end	finish success=yes
29840033	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
29842399	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2817
29844278	worker	spike_loop_gap	after_loop=0 gap_us=120135	120135
29886474	worker	spike_loop_duration	iter=0	42192
29887638	ui	ui_fast_repeat_flush	pending_iters=1	665
29941541	worker	loop_interval_sleep	requested_ms=54	55021
29941546	worker	spike_loop_gap	after_loop=0 gap_us=55073	55073
29947672	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	2056
29978014	worker	spike_loop_duration	iter=1 loop=1	36464
29979129	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	1010
29979188	ui	session_end	finish success=yes loop=1
30003939	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
30006754	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3289
30008419	worker	spike_loop_gap	after_loop=1 gap_us=30405	30405
30047797	worker	spike_loop_duration	iter=0	39376
30048401	ui	ui_fast_repeat_flush	pending_iters=1	402
30106633	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	948
30109768	ui	session_end	finish success=yes
30174892	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
30177666	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3221
30179398	worker	spike_loop_gap	after_loop=0 gap_us=131599	131599
30216237	worker	spike_loop_duration	iter=0	36834
30216943	ui	ui_fast_repeat_flush	pending_iters=1	417
30287546	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1039
30290868	ui	session_end	finish success=yes
30381358	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
30383726	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2749
30385456	worker	spike_loop_gap	after_loop=0 gap_us=169218	169218
30425312	worker	spike_loop_duration	iter=0	39852
30426181	ui	ui_fast_repeat_flush	pending_iters=1	418
30488729	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1137
30497011	ui	session_end	finish success=yes
32696317	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
32698883	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3038
32700587	worker	spike_loop_gap	after_loop=0 gap_us=2275276	2275276
32701192	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
32701734	ui	refreshWorkflowEditor	elapsed_ms=2	2000
32738718	worker	spike_loop_duration	iter=0	38129
32739366	ui	ui_fast_repeat_flush	pending_iters=1	422
32793009	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1321
32795246	ui	session_end	finish success=yes
32839811	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
32841896	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2433
32843462	worker	spike_loop_gap	after_loop=0 gap_us=104743	104743
32844016	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
32844417	ui	refreshWorkflowEditor	elapsed_ms=1	1000
32886808	worker	spike_loop_duration	iter=0	43344
32887537	ui	ui_fast_repeat_flush	pending_iters=1	426
32932698	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1072
32940256	ui	session_end	finish success=yes
32993246	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
32996593	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3906
32998230	worker	spike_loop_gap	after_loop=0 gap_us=111420	111420
33035999	worker	spike_loop_duration	iter=0	37764
33036886	ui	ui_fast_repeat_flush	pending_iters=1	387
33096720	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1624
33099468	ui	session_end	finish success=yes
33136117	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
33138190	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2476
33139921	worker	spike_loop_gap	after_loop=0 gap_us=103924	103924
33176554	worker	spike_loop_duration	iter=0	36629
33177616	ui	ui_fast_repeat_flush	pending_iters=1	439
33257538	worker	loop_interval_sleep	requested_ms=79	80894
33257545	worker	spike_loop_gap	after_loop=0 gap_us=80991	80991
33275838	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1146
33293759	worker	spike_loop_duration	iter=1 loop=1	36211
33294171	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	345
33294349	ui	session_end	finish success=yes loop=1
35256839	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
35259279	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2942
35261052	worker	spike_loop_gap	after_loop=1 gap_us=1967293	1967293
35261518	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
35261899	ui	refreshWorkflowEditor	elapsed_ms=1	1000
35298774	worker	spike_loop_duration	iter=0	37718
35299399	ui	ui_fast_repeat_flush	pending_iters=1	414
35380725	worker	loop_interval_sleep	requested_ms=80	81843
35380730	worker	spike_loop_gap	after_loop=0 gap_us=81957	81957
35417131	worker	spike_loop_duration	iter=1 loop=1	36399
35417530	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	339
35427637	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1063
35427995	ui	session_end	finish success=yes loop=1
35659575	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
35662383	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3331
35663896	worker	spike_loop_gap	after_loop=1 gap_us=246763	246763
35664446	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
35665040	ui	refreshWorkflowEditor	elapsed_ms=2	2000
35701712	worker	spike_loop_duration	iter=0	37812
35702405	ui	ui_fast_repeat_flush	pending_iters=1	471
35755162	worker	loop_interval_sleep	requested_ms=52	53343
35755168	worker	spike_loop_gap	after_loop=0 gap_us=53456	53456
35778259	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1820
35791354	worker	spike_loop_duration	iter=1 loop=1	36182
35791759	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	339
35791900	ui	session_end	finish success=yes loop=1
35900007	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
35903099	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3591
35904877	worker	spike_loop_gap	after_loop=1 gap_us=113523	113523
35905414	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
35905816	ui	refreshWorkflowEditor	elapsed_ms=1	1000
35943489	worker	spike_loop_duration	iter=0	38608
35944866	ui	ui_fast_repeat_flush	pending_iters=1	547
35994908	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1541
35995128	ui	session_end	finish success=yes
36086364	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
36088295	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2294
36089750	worker	spike_loop_gap	after_loop=0 gap_us=146262	146262
36129383	worker	spike_loop_duration	iter=0	39628
36130956	ui	ui_fast_repeat_flush	pending_iters=1	404
36179733	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1326
36181922	ui	session_end	finish success=yes
36246775	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
36249522	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3152
36251358	worker	spike_loop_gap	after_loop=0 gap_us=121975	121975
36289159	worker	spike_loop_duration	iter=0	37797
36290147	ui	ui_fast_repeat_flush	pending_iters=1	693
36369523	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1171
36371930	ui	session_end	finish success=yes
36908029	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
36913878	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	6312
36915539	worker	spike_loop_gap	after_loop=0 gap_us=626380	626380
36916168	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
36916539	ui	refreshWorkflowEditor	elapsed_ms=2	2000
36957342	worker	spike_loop_duration	iter=0	41799
36958115	ui	ui_fast_repeat_flush	pending_iters=1	446
36992369	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1260
36998446	ui	session_end	finish success=yes
37067037	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
37070106	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3656
37071638	worker	spike_loop_gap	after_loop=0 gap_us=114298	114298
37108131	worker	spike_loop_duration	iter=0	36488
37108696	ui	ui_fast_repeat_flush	pending_iters=1	403
37162529	worker	loop_interval_sleep	requested_ms=53	54308
37162534	worker	spike_loop_gap	after_loop=0 gap_us=54405	54405
37165377	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	783
37200220	worker	spike_loop_duration	iter=1 loop=1	37682
37200647	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	346
37200681	ui	session_end	finish success=yes loop=1
37235651	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
37237762	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2453
37239541	worker	spike_loop_gap	after_loop=1 gap_us=39321	39321
37275843	worker	spike_loop_duration	iter=0	36298
37276569	ui	ui_fast_repeat_flush	pending_iters=1	416
37336307	worker	loop_interval_sleep	requested_ms=59	60266
37336315	worker	spike_loop_gap	after_loop=0 gap_us=60473	60473
37343310	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1189
37373917	worker	spike_loop_duration	iter=1 loop=1	37599
37374488	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	392
37374493	ui	session_end	finish success=yes loop=1
38524484	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
38527161	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3203
38528617	worker	spike_loop_gap	after_loop=1 gap_us=1154700	1154700
38529145	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
38529516	ui	refreshWorkflowEditor	elapsed_ms=1	1000
38566193	worker	spike_loop_duration	iter=0	37572
38566881	ui	ui_fast_repeat_flush	pending_iters=1	404
38608483	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1042
38617387	ui	session_end	finish success=yes
38663159	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
38665647	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2953
38667375	worker	spike_loop_gap	after_loop=0 gap_us=101182	101182
38704011	worker	spike_loop_duration	iter=0	36632
38704578	ui	ui_fast_repeat_flush	pending_iters=1	404
38765191	ui	session_end	reason=preempted_by_new_session
38765246	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
38765261	worker	loop_interval_sleep	requested_ms=60	61162
38768102	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	3382
38771139	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
38771610	ui	refreshWorkflowEditor	elapsed_ms=1	1000
38783234	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1276
38804527	worker	spike_loop_duration	iter=1	39262
38804658	ui	ui_fast_repeat_flush	pending_iters=1	38
38804765	ui	session_end	finish success=yes
38881075	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1032
38927573	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
38930135	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3042
38931869	worker	spike_loop_gap	after_loop=1 gap_us=127339	127339
38969357	worker	spike_loop_duration	iter=0	37484
38970598	ui	ui_fast_repeat_flush	pending_iters=1	573
39026999	worker	loop_interval_sleep	requested_ms=56	57467
39027005	worker	spike_loop_gap	after_loop=0 gap_us=57650	57650
39059368	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1912
39066038	worker	spike_loop_duration	iter=1 loop=1	38978
39067276	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	480
39067281	ui	session_end	finish success=yes loop=1
41747412	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
41750038	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3082
41751656	worker	spike_loop_gap	after_loop=1 gap_us=2685671	2685671
41752078	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
41752477	ui	refreshWorkflowEditor	elapsed_ms=2	2000
41789041	worker	spike_loop_duration	iter=0	37382
41789704	ui	ui_fast_repeat_flush	pending_iters=1	486
41856221	worker	loop_interval_sleep	requested_ms=66	67092
41856228	worker	spike_loop_gap	after_loop=0 gap_us=67187	67187
41892636	worker	spike_loop_duration	iter=1 loop=1	36405
41893171	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	410
41948741	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1448
41955155	ui	session_end	finish success=yes loop=1
42253888	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
42256724	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3444
42258414	worker	spike_loop_gap	after_loop=1 gap_us=365777	365777
42259223	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
42259711	ui	refreshWorkflowEditor	elapsed_ms=2	2000
42296696	worker	spike_loop_duration	iter=0	38278
42297339	ui	ui_fast_repeat_flush	pending_iters=1	447
42377236	worker	loop_interval_sleep	requested_ms=79	80426
42377243	worker	spike_loop_gap	after_loop=0 gap_us=80547	80547
42396516	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1135
42413338	worker	spike_loop_duration	iter=1 loop=1	36093
42413992	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	596
42414181	ui	session_end	finish success=yes loop=1
42512968	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
42515445	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2968
42517017	worker	spike_loop_gap	after_loop=1 gap_us=103678	103678
42517546	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
42518096	ui	refreshWorkflowEditor	elapsed_ms=2	2000
42555005	worker	spike_loop_duration	iter=0	37984
42555682	ui	ui_fast_repeat_flush	pending_iters=1	407
42622215	worker	loop_interval_sleep	requested_ms=66	67117
42622221	worker	spike_loop_gap	after_loop=0 gap_us=67217	67217
42637042	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1999
42659246	worker	spike_loop_duration	iter=1 loop=1	37022
42659740	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	384
42660382	ui	session_end	finish success=yes loop=1
42694411	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
42696789	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2838
42698415	worker	spike_loop_gap	after_loop=1 gap_us=39168	39168
42735888	worker	spike_loop_duration	iter=0	37471
42736535	ui	ui_fast_repeat_flush	pending_iters=1	366
42798595	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1125
42798669	ui	session_end	finish success=yes
42842888	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
42845868	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3501
42847469	worker	spike_loop_gap	after_loop=0 gap_us=111578	111578
42886667	worker	spike_loop_duration	iter=0	39197
42887529	ui	ui_fast_repeat_flush	pending_iters=1	388
42964416	worker	loop_interval_sleep	requested_ms=76	77698
42964423	worker	spike_loop_gap	after_loop=0 gap_us=77754	77754
42979074	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1249
43000686	worker	spike_loop_duration	iter=1 loop=1	36260
43001920	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	1046
43002094	ui	session_end	finish success=yes loop=1
45789103	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
45791243	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2554
45793023	worker	spike_loop_gap	after_loop=1 gap_us=2792337	2792337
45793812	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
45794188	ui	refreshWorkflowEditor	elapsed_ms=2	2000
45830744	worker	spike_loop_duration	iter=0	37717
45831331	ui	ui_fast_repeat_flush	pending_iters=1	390
45899962	worker	loop_interval_sleep	requested_ms=68	69152
45899968	worker	spike_loop_gap	after_loop=0 gap_us=69224	69224
45936662	worker	spike_loop_duration	iter=1 loop=1	36691
45937126	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	413
45972622	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1364
45977479	ui	session_end	finish success=yes loop=1
46556104	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
46559147	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3551
46560730	worker	spike_loop_gap	after_loop=1 gap_us=624067	624067
46561308	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
46561770	ui	refreshWorkflowEditor	elapsed_ms=1	1000
46598845	worker	spike_loop_duration	iter=0	38113
46599565	ui	ui_fast_repeat_flush	pending_iters=1	411
46648639	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1196
46651406	ui	session_end	finish success=yes
46704821	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
46707312	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2965
46708749	worker	spike_loop_gap	after_loop=0 gap_us=109902	109902
46748329	worker	spike_loop_duration	iter=0	39579
46749559	ui	ui_fast_repeat_flush	pending_iters=1	609
46806764	worker	loop_interval_sleep	requested_ms=57	58295
46806769	worker	spike_loop_gap	after_loop=0 gap_us=58439	58439
46826626	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1175
46842907	worker	spike_loop_duration	iter=1 loop=1	36134
46843318	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	357
46843483	ui	session_end	finish success=yes loop=1
47639028	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
47641431	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2847
47643236	worker	spike_loop_gap	after_loop=1 gap_us=800328	800328
47643836	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
47644272	ui	refreshWorkflowEditor	elapsed_ms=2	2000
47681146	worker	spike_loop_duration	iter=0	37906
47681756	ui	ui_fast_repeat_flush	pending_iters=1	413
47735589	worker	loop_interval_sleep	requested_ms=53	54345
47735595	worker	spike_loop_gap	after_loop=0 gap_us=54449	54449
47772224	worker	spike_loop_duration	iter=1 loop=1	36626
47772688	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	393
47778339	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1276
47786379	ui	session_end	finish success=yes loop=1
48498685	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
48501633	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3525
48503082	worker	spike_loop_gap	after_loop=1 gap_us=730858	730858
48503925	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
48504360	ui	refreshWorkflowEditor	elapsed_ms=2	2000
48541164	worker	spike_loop_duration	iter=0	38078
48541924	ui	ui_fast_repeat_flush	pending_iters=1	449
48595320	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1374
48600487	ui	session_end	finish success=yes
48677914	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
48680716	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3470
48682326	worker	spike_loop_gap	after_loop=0 gap_us=141161	141161
48721446	worker	spike_loop_duration	iter=0	39115
48722077	ui	ui_fast_repeat_flush	pending_iters=1	400
48755418	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1118
48762704	ui	session_end	finish success=yes
48831276	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
48834039	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3210
48835594	worker	spike_loop_gap	after_loop=0 gap_us=114148	114148
48874746	worker	spike_loop_duration	iter=0	39148
48875279	ui	ui_fast_repeat_flush	pending_iters=1	336
48918277	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1298
48925879	ui	session_end	finish success=yes
48981161	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
48983815	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3118
48985406	worker	spike_loop_gap	after_loop=0 gap_us=110659	110659
49026002	worker	spike_loop_duration	iter=0	40592
49026824	ui	ui_fast_repeat_flush	pending_iters=1	426
49084581	worker	loop_interval_sleep	requested_ms=57	58393
49084588	worker	spike_loop_gap	after_loop=0 gap_us=58587	58587
49096369	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1584
49122997	worker	spike_loop_duration	iter=1 loop=1	38405
49123507	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	416
49123655	ui	session_end	finish success=yes loop=1
49143807	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
49146273	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2960
49148032	worker	spike_loop_gap	after_loop=1 gap_us=25033	25033
49149155	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
49149559	ui	refreshWorkflowEditor	elapsed_ms=1	1000
49190159	worker	spike_loop_duration	iter=0	42125
49190999	ui	ui_fast_repeat_flush	pending_iters=1	669
49260864	worker	loop_interval_sleep	requested_ms=69	70618
49260871	worker	spike_loop_gap	after_loop=0 gap_us=70711	70711
49271488	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2057
49297082	worker	spike_loop_duration	iter=1 loop=1	36209
49297531	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	358
49297592	ui	session_end	finish success=yes loop=1
49320291	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
49322884	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3060
49324599	worker	spike_loop_gap	after_loop=1 gap_us=27516	27516
49361225	worker	spike_loop_duration	iter=0	36608
49361925	ui	ui_fast_repeat_flush	pending_iters=1	416
49413542	worker	loop_interval_sleep	requested_ms=51	52233
49413549	worker	spike_loop_gap	after_loop=0 gap_us=52324	52324
49419563	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1089
49453417	worker	spike_loop_duration	iter=1 loop=1	39865
49453879	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	368
49454375	ui	session_end	finish success=yes loop=1
49470492	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
49479106	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	9122
49481382	worker	spike_loop_gap	after_loop=1 gap_us=27963	27963
49519058	worker	spike_loop_duration	iter=0	37672
49519781	ui	ui_fast_repeat_flush	pending_iters=1	390
49577267	worker	loop_interval_sleep	requested_ms=57	58109
49577273	worker	spike_loop_gap	after_loop=0 gap_us=58216	58216
49611829	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2096
49614414	worker	spike_loop_duration	iter=1 loop=1	37138
49614855	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	244
49614859	ui	session_end	finish success=yes loop=1
49936417	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
49938755	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2696
49940878	worker	spike_loop_gap	after_loop=1 gap_us=326463	326463
49941340	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
49942083	ui	refreshWorkflowEditor	elapsed_ms=2	2000
49979321	worker	spike_loop_duration	iter=0	38438
49980018	ui	ui_fast_repeat_flush	pending_iters=1	457
50023573	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1090
50030656	ui	session_end	finish success=yes
50086062	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
50088277	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2642
50089872	worker	spike_loop_gap	after_loop=0 gap_us=110551	110551
50126947	worker	spike_loop_duration	iter=0	37071
50127622	ui	ui_fast_repeat_flush	pending_iters=1	394
50188358	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	990
50188517	ui	session_end	finish success=yes
50243122	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
50246106	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3449
50247578	worker	spike_loop_gap	after_loop=0 gap_us=120632	120632
50288544	worker	spike_loop_duration	iter=0	40962
50289764	ui	ui_fast_repeat_flush	pending_iters=1	399
50359376	worker	loop_interval_sleep	requested_ms=69	70763
50359384	worker	spike_loop_gap	after_loop=0 gap_us=70840	70840
50388288	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1651
50396442	worker	spike_loop_duration	iter=1 loop=1	37056
50397096	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	540
50397100	ui	session_end	finish success=yes loop=1
58655642	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
58658343	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3230
58660672	worker	spike_loop_gap	after_loop=1 gap_us=8264227	8264227
58698250	worker	spike_loop_duration	iter=0	37573
58699286	ui	ui_fast_repeat_flush	pending_iters=1	630
58767821	worker	loop_interval_sleep	requested_ms=68	69437
58767830	worker	spike_loop_gap	after_loop=0 gap_us=69582	69582
58801589	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1629
58804557	worker	spike_loop_duration	iter=1 loop=1	36725
58805382	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	368
58805387	ui	session_end	finish success=yes loop=1
60190933	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
60193481	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2949
60195174	worker	spike_loop_gap	after_loop=1 gap_us=1390616	1390616
60196468	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
60196866	ui	refreshWorkflowEditor	elapsed_ms=1	1000
60236248	worker	spike_loop_duration	iter=0	41072
60237832	ui	ui_fast_repeat_flush	pending_iters=1	634
60278961	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1253
60287148	ui	session_end	finish success=yes
60343792	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
60345971	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2621
60347447	worker	spike_loop_gap	after_loop=0 gap_us=111198	111198
60386456	worker	spike_loop_duration	iter=0	39007
60387386	ui	ui_fast_repeat_flush	pending_iters=1	404
60466240	worker	loop_interval_sleep	requested_ms=78	79734
60466247	worker	spike_loop_gap	after_loop=0 gap_us=79789	79789
60503030	worker	spike_loop_duration	iter=1 loop=1	36780
60503500	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	345
60505151	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	778
60513616	ui	session_end	finish success=yes loop=1
60970375	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
60972789	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2922
60974332	worker	spike_loop_gap	after_loop=1 gap_us=471301	471301
60974906	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
60975546	ui	refreshWorkflowEditor	elapsed_ms=2	2000
61012289	worker	spike_loop_duration	iter=0	37953
61013461	ui	ui_fast_repeat_flush	pending_iters=1	905
61071247	worker	loop_interval_sleep	requested_ms=58	58858
61071254	worker	spike_loop_gap	after_loop=0 gap_us=58966	58966
61094353	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1372
61107363	worker	spike_loop_duration	iter=1 loop=1	36107
61107833	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	375
61108017	ui	session_end	finish success=yes loop=1
61456871	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
61460955	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4672
61462853	worker	spike_loop_gap	after_loop=1 gap_us=355488	355488
61500037	worker	spike_loop_duration	iter=0	37182
61500837	ui	ui_fast_repeat_flush	pending_iters=1	596
61555291	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1091
61560535	ui	session_end	finish success=yes
61606775	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
61609186	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2919
61610708	worker	spike_loop_gap	after_loop=0 gap_us=110669	110669
61650225	worker	spike_loop_duration	iter=0	39515
61651176	ui	ui_fast_repeat_flush	pending_iters=1	685
61712923	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1304
61722028	ui	session_end	finish success=yes
62058080	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
62061324	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3783
62062994	worker	spike_loop_gap	after_loop=0 gap_us=412766	412766
62099130	worker	spike_loop_duration	iter=0	36132
62099732	ui	ui_fast_repeat_flush	pending_iters=1	408
62133118	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1090
62140363	ui	session_end	finish success=yes
62175382	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
62178426	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3689
62180127	worker	spike_loop_gap	after_loop=0 gap_us=80996	80996
62220068	worker	spike_loop_duration	iter=0	39937
62220770	ui	ui_fast_repeat_flush	pending_iters=1	510
62278342	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1085
62281548	ui	session_end	finish success=yes
62315809	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
62319063	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3724
62320583	worker	spike_loop_gap	after_loop=0 gap_us=100516	100516
62357125	worker	spike_loop_duration	iter=0	36539
62357837	ui	ui_fast_repeat_flush	pending_iters=1	412
62429857	worker	loop_interval_sleep	requested_ms=71	72635
62429863	worker	spike_loop_gap	after_loop=0 gap_us=72739	72739
62461662	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1315
62467541	worker	spike_loop_duration	iter=1 loop=1	37674
62468044	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	426
62468529	ui	session_end	finish success=yes loop=1
62541135	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
62544413	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3742
62545897	worker	spike_loop_gap	after_loop=1 gap_us=78356	78356
62546415	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
62546780	ui	refreshWorkflowEditor	elapsed_ms=1	1000
62586265	worker	spike_loop_duration	iter=0	40366
62586933	ui	ui_fast_repeat_flush	pending_iters=1	403
62649806	worker	loop_interval_sleep	requested_ms=62	63497
62649813	worker	spike_loop_gap	after_loop=0 gap_us=63548	63548
62653176	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1441
62686324	worker	spike_loop_duration	iter=1 loop=1	36508
62687256	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	556
62687261	ui	session_end	finish success=yes loop=1
62747291	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
62750383	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3597
62752170	worker	spike_loop_gap	after_loop=1 gap_us=65844	65844
62788473	worker	spike_loop_duration	iter=0	36298
62789322	ui	ui_fast_repeat_flush	pending_iters=1	522
62857892	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1060
62860605	ui	session_end	finish success=yes
63418252	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
63421150	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3410
63422850	worker	spike_loop_gap	after_loop=0 gap_us=634376	634376
63423361	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
63423773	ui	refreshWorkflowEditor	elapsed_ms=2	2000
63460826	worker	spike_loop_duration	iter=0	37972
63461401	ui	ui_fast_repeat_flush	pending_iters=1	407
63521059	worker	loop_interval_sleep	requested_ms=59	60136
63521065	worker	spike_loop_gap	after_loop=0 gap_us=60241	60241
63523988	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1289
63557340	worker	spike_loop_duration	iter=1 loop=1	36272
63557763	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	357
63557796	ui	session_end	finish success=yes loop=1
63577600	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
63580748	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3650
63582517	worker	spike_loop_gap	after_loop=1 gap_us=25177	25177
63621960	worker	spike_loop_duration	iter=0	39442
63622784	ui	ui_fast_repeat_flush	pending_iters=1	369
63673587	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1118
63688051	ui	session_end	finish success=yes
63714417	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
63716916	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2956
63718540	worker	spike_loop_gap	after_loop=0 gap_us=96578	96578
63719129	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
63719578	ui	refreshWorkflowEditor	elapsed_ms=1	1000
63756913	worker	spike_loop_duration	iter=0	38368
63757542	ui	ui_fast_repeat_flush	pending_iters=1	415
63812036	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1159
63817091	ui	session_end	finish success=yes
63869237	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
63872011	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3605
63873786	worker	spike_loop_gap	after_loop=0 gap_us=116874	116874
63913894	worker	spike_loop_duration	iter=0	40106
63914952	ui	ui_fast_repeat_flush	pending_iters=1	764
63956830	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1255
63965101	ui	session_end	finish success=yes
64009097	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
64011558	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3175
64013256	worker	spike_loop_gap	after_loop=0 gap_us=99361	99361
64054206	worker	spike_loop_duration	iter=0	40946
64055029	ui	ui_fast_repeat_flush	pending_iters=1	338
64112214	worker	loop_interval_sleep	requested_ms=57	57901
64112221	worker	spike_loop_gap	after_loop=0 gap_us=58015	58015
64129989	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1202
64148607	worker	spike_loop_duration	iter=1 loop=1	36383
64149068	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	375
64149102	ui	session_end	finish success=yes loop=1
64411840	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
64414220	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2861
64416175	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
64416606	worker	spike_loop_gap	after_loop=1 gap_us=267999	267999
64416912	ui	refreshWorkflowEditor	elapsed_ms=2	2000
64456678	worker	spike_loop_duration	iter=0	40067
64457495	ui	ui_fast_repeat_flush	pending_iters=1	417
64473486	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1635
64482691	ui	session_end	finish success=yes
64527804	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
64530246	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2910
64531781	worker	spike_loop_gap	after_loop=0 gap_us=75104	75104
64571478	worker	spike_loop_duration	iter=0	39693
64572227	ui	ui_fast_repeat_flush	pending_iters=1	433
64622667	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	974
64623025	ui	session_end	finish success=yes
64696380	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
64698551	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2583
64700167	worker	spike_loop_gap	after_loop=0 gap_us=128690	128690
64736915	worker	spike_loop_duration	iter=0	36744
64737482	ui	ui_fast_repeat_flush	pending_iters=1	422
64788334	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	948
64788480	ui	session_end	finish success=yes
64823159	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
64825483	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3185
64827255	worker	spike_loop_gap	after_loop=0 gap_us=90339	90339
64866762	worker	spike_loop_duration	iter=0	39503
64867495	ui	ui_fast_repeat_flush	pending_iters=1	561
64927849	worker	loop_interval_sleep	requested_ms=60	60964
64927856	worker	spike_loop_gap	after_loop=0 gap_us=61095	61095
64958242	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1156
64964680	worker	spike_loop_duration	iter=1 loop=1	36822
64965078	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	300
64965131	ui	session_end	finish success=yes loop=1
70013795	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
70016346	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3016
70018248	worker	spike_loop_gap	after_loop=1 gap_us=5053567	5053567
70018837	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
70019395	ui	refreshWorkflowEditor	elapsed_ms=2	2000
70056986	worker	spike_loop_duration	iter=0	38730
70057974	ui	ui_fast_repeat_flush	pending_iters=1	551
70137832	worker	loop_interval_sleep	requested_ms=79	80764
70137838	worker	spike_loop_gap	after_loop=0 gap_us=80858	80858
70174044	worker	spike_loop_duration	iter=1 loop=1	36204
70174426	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	329
70193519	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1046
70196521	ui	session_end	finish success=yes loop=1
70278257	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
70281015	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3302
70282750	worker	spike_loop_gap	after_loop=1 gap_us=108704	108704
70283361	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
70283908	ui	refreshWorkflowEditor	elapsed_ms=2	2000
70320936	worker	spike_loop_duration	iter=0	38183
70321462	ui	ui_fast_repeat_flush	pending_iters=1	358
70374185	worker	loop_interval_sleep	requested_ms=52	53190
70374190	worker	spike_loop_gap	after_loop=0 gap_us=53253	53253
70404325	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1091
70410843	worker	spike_loop_duration	iter=1 loop=1	36651
70411427	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	519
70411560	ui	session_end	finish success=yes loop=1
70461694	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
70466123	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	5469
70467959	worker	spike_loop_gap	after_loop=1 gap_us=57114	57114
70469133	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
70469633	ui	refreshWorkflowEditor	elapsed_ms=2	2000
70507137	worker	spike_loop_duration	iter=0	39174
70507788	ui	ui_fast_repeat_flush	pending_iters=1	457
70579079	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1389
70587490	ui	session_end	finish success=yes
70648229	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
70650498	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2755
70652353	worker	spike_loop_gap	after_loop=0 gap_us=145216	145216
70691544	worker	spike_loop_duration	iter=0	39185
70693762	ui	ui_fast_repeat_flush	pending_iters=1	367
70739361	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1547
70743023	ui	session_end	finish success=yes
70796829	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
70799227	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2873
70801502	worker	spike_loop_gap	after_loop=0 gap_us=109960	109960
70842861	worker	spike_loop_duration	iter=0	41355
70843467	ui	ui_fast_repeat_flush	pending_iters=1	433
70909700	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1445
70913714	ui	session_end	finish success=yes
71476929	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
71479242	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2790
71480799	worker	spike_loop_gap	after_loop=0 gap_us=637937	637937
71481439	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
71481853	ui	refreshWorkflowEditor	elapsed_ms=2	2000
71518916	worker	spike_loop_duration	iter=0	38111
71519520	ui	ui_fast_repeat_flush	pending_iters=1	424
71574507	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1063
71580305	ui	session_end	finish success=yes
71636788	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
71638886	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2406
71640486	worker	spike_loop_gap	after_loop=0 gap_us=121573	121573
71680709	worker	spike_loop_duration	iter=0	40221
71681631	ui	ui_fast_repeat_flush	pending_iters=1	667
71737711	worker	loop_interval_sleep	requested_ms=56	56890
71737718	worker	spike_loop_gap	after_loop=0 gap_us=57008	57008
71762915	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1433
71773790	worker	spike_loop_duration	iter=1 loop=1	36070
71774212	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	361
71774243	ui	session_end	finish success=yes loop=1
71855853	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
71858848	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3497
71860611	worker	spike_loop_gap	after_loop=1 gap_us=86821	86821
71897290	worker	spike_loop_duration	iter=0	36673
71898297	ui	ui_fast_repeat_flush	pending_iters=1	671
71952695	worker	loop_interval_sleep	requested_ms=54	55318
71952700	worker	spike_loop_gap	after_loop=0 gap_us=55415	55415
71990784	worker	spike_loop_duration	iter=1 loop=1	38081
71991625	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	684
71997114	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	2001
72001396	ui	session_end	finish success=yes loop=1
73924165	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
73926820	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3119
73928617	worker	spike_loop_gap	after_loop=1 gap_us=1937832	1937832
73929238	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
73929622	ui	refreshWorkflowEditor	elapsed_ms=2	2000
73966394	worker	spike_loop_duration	iter=0	37773
73967248	ui	ui_fast_repeat_flush	pending_iters=1	464
74032258	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1425
74038819	ui	session_end	finish success=yes
74126345	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
74130780	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	5008
74133738	worker	spike_loop_gap	after_loop=0 gap_us=167343	167343
74134315	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
74134846	ui	refreshWorkflowEditor	elapsed_ms=2	2000
74176569	worker	spike_loop_duration	iter=0	42826
74177555	ui	ui_fast_repeat_flush	pending_iters=1	444
74225644	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1424
74228158	ui	session_end	finish success=yes
74314514	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
74317266	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3680
74319735	worker	spike_loop_gap	after_loop=0 gap_us=143164	143164
74359390	worker	spike_loop_duration	iter=0	39652
74360204	ui	ui_fast_repeat_flush	pending_iters=1	472
74404013	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1182
74411670	ui	session_end	finish success=yes
74461974	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
74464473	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2914
74466775	worker	spike_loop_gap	after_loop=0 gap_us=107382	107382
74506916	worker	spike_loop_duration	iter=0	40137
74507520	ui	ui_fast_repeat_flush	pending_iters=1	383
74563985	worker	loop_interval_sleep	requested_ms=56	57001
74563991	worker	spike_loop_gap	after_loop=0 gap_us=57076	57076
74600536	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1581
74601028	worker	spike_loop_duration	iter=1 loop=1	37035
74601655	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	419
74601658	ui	session_end	finish success=yes loop=1
75094924	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
75097733	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3304
75099318	worker	spike_loop_gap	after_loop=1 gap_us=498289	498289
75099995	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
75100425	ui	refreshWorkflowEditor	elapsed_ms=1	1000
75141279	worker	spike_loop_duration	iter=0	41957
75141854	ui	ui_fast_repeat_flush	pending_iters=1	419
75181771	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1065
75188313	ui	session_end	finish success=yes
75255005	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
75257459	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2923
75259968	worker	spike_loop_gap	after_loop=0 gap_us=118688	118688
75301897	worker	spike_loop_duration	iter=0	41924
75302853	ui	ui_fast_repeat_flush	pending_iters=1	614
75352669	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1429
75354869	ui	session_end	finish success=yes
75401090	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
75404161	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3596
75405573	worker	spike_loop_gap	after_loop=0 gap_us=103676	103676
75441777	worker	spike_loop_duration	iter=0	36200
75444091	ui	ui_fast_repeat_flush	pending_iters=1	561
75511455	worker	loop_interval_sleep	requested_ms=68	69582
75511461	worker	spike_loop_gap	after_loop=0 gap_us=69685	69685
75537233	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1184
75547634	worker	spike_loop_duration	iter=1 loop=1	36170
75548080	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	331
75548085	ui	session_end	finish success=yes loop=1
81504247	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
81506094	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2300
81507811	worker	spike_loop_gap	after_loop=1 gap_us=5960177	5960177
81545648	worker	spike_loop_duration	iter=0	37832
81546333	ui	ui_fast_repeat_flush	pending_iters=1	517
81627447	worker	loop_interval_sleep	requested_ms=80	81708
81627454	worker	spike_loop_gap	after_loop=0 gap_us=81808	81808
81660959	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1587
81664189	worker	spike_loop_duration	iter=1 loop=1	36732
81664680	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	313
81664684	ui	session_end	finish success=yes loop=1
84113894	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
84116740	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3341
84118665	worker	spike_loop_gap	after_loop=1 gap_us=2454475	2454475
84119549	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
84119972	ui	refreshWorkflowEditor	elapsed_ms=2	2000
84157014	worker	spike_loop_duration	iter=0	38346
84157803	ui	ui_fast_repeat_flush	pending_iters=1	491
84233748	worker	loop_interval_sleep	requested_ms=75	76630
84233756	worker	spike_loop_gap	after_loop=0 gap_us=76740	76740
84274338	worker	spike_loop_duration	iter=1 loop=1	40580
84274722	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	334
84328919	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1193
84345874	ui	session_end	finish success=yes loop=1
84727954	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
84730442	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3001
84731982	worker	spike_loop_gap	after_loop=1 gap_us=457642	457642
84732612	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
84733067	ui	refreshWorkflowEditor	elapsed_ms=1	1000
84770009	worker	spike_loop_duration	iter=0	38025
84770722	ui	ui_fast_repeat_flush	pending_iters=1	499
84833069	worker	loop_interval_sleep	requested_ms=62	62968
84833075	worker	spike_loop_gap	after_loop=0 gap_us=63065	63065
84871804	worker	spike_loop_duration	iter=1 loop=1	38726
84872626	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	749
84906353	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1877
84912568	ui	session_end	finish success=yes loop=1
85243465	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
85246611	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3686
85248172	worker	spike_loop_gap	after_loop=1 gap_us=376367	376367
85248788	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
85249205	ui	refreshWorkflowEditor	elapsed_ms=1	1000
85286867	worker	spike_loop_duration	iter=0	38693
85287831	ui	ui_fast_repeat_flush	pending_iters=1	429
85341464	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1125
85348423	ui	session_end	finish success=yes
85417375	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
85420538	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3685
85422014	worker	spike_loop_gap	after_loop=0 gap_us=135145	135145
85458779	worker	spike_loop_duration	iter=0	36763
85459480	ui	ui_fast_repeat_flush	pending_iters=1	461
85509889	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1485
85510125	ui	session_end	finish success=yes
85924753	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
85927130	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2872
85928712	worker	spike_loop_gap	after_loop=0 gap_us=469932	469932
85929293	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
85929875	ui	refreshWorkflowEditor	elapsed_ms=1	1000
85966599	worker	spike_loop_duration	iter=0	37883
85967214	ui	ui_fast_repeat_flush	pending_iters=1	433
86013662	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1010
86019588	ui	session_end	finish success=yes
86072969	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
86075913	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3443
86077446	worker	spike_loop_gap	after_loop=0 gap_us=110847	110847
86114215	worker	spike_loop_duration	iter=0	36764
86114987	ui	ui_fast_repeat_flush	pending_iters=1	406
86180344	worker	loop_interval_sleep	requested_ms=65	66055
86180351	worker	spike_loop_gap	after_loop=0 gap_us=66137	66137
86187629	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1011
86216533	worker	spike_loop_duration	iter=1 loop=1	36179
86216971	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	334
86217002	ui	session_end	finish success=yes loop=1
86256508	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
86259063	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3059
86261152	worker	spike_loop_gap	after_loop=1 gap_us=44618	44618
86300740	worker	spike_loop_duration	iter=0	39581
86301488	ui	ui_fast_repeat_flush	pending_iters=1	438
86358013	worker	loop_interval_sleep	requested_ms=56	57181
86358023	worker	spike_loop_gap	after_loop=0 gap_us=57284	57284
86376061	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1781
86394775	worker	spike_loop_duration	iter=1 loop=1	36750
86395972	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	1130
86396065	ui	session_end	finish success=yes loop=1
87858089	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
87860864	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3400
87862374	worker	spike_loop_gap	after_loop=1 gap_us=1467598	1467598
87862986	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
87863403	ui	refreshWorkflowEditor	elapsed_ms=1	1000
87900340	worker	spike_loop_duration	iter=0	37962
87901067	ui	ui_fast_repeat_flush	pending_iters=1	416
87960875	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1467
87962968	ui	session_end	finish success=yes
87986311	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
87989634	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3671
87991355	worker	spike_loop_gap	after_loop=0 gap_us=91014	91014
87992165	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
87992656	ui	refreshWorkflowEditor	elapsed_ms=2	2000
88036948	worker	spike_loop_duration	iter=0	45591
88037785	ui	ui_fast_repeat_flush	pending_iters=1	409
88075660	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1448
88077793	ui	session_end	finish success=yes
88137235	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
88139069	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2197
88140660	worker	spike_loop_gap	after_loop=0 gap_us=103711	103711
88181732	worker	spike_loop_duration	iter=0	41068
88182670	ui	ui_fast_repeat_flush	pending_iters=1	685
88238473	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1071
88243361	ui	session_end	finish success=yes
88274862	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
88277819	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3599
88279760	worker	spike_loop_gap	after_loop=0 gap_us=98030	98030
88317382	worker	spike_loop_duration	iter=0	37618
88318526	ui	ui_fast_repeat_flush	pending_iters=1	586
88368413	worker	loop_interval_sleep	requested_ms=50	50909
88368420	worker	spike_loop_gap	after_loop=0 gap_us=51037	51037
88393959	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1627
88405115	worker	spike_loop_duration	iter=1 loop=1	36692
88405473	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	304
88405563	ui	session_end	finish success=yes loop=1
89017100	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
89019848	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3243
89021641	worker	spike_loop_gap	after_loop=1 gap_us=616525	616525
89022243	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
89022642	ui	refreshWorkflowEditor	elapsed_ms=2	2000
89059420	worker	spike_loop_duration	iter=0	37775
89060753	ui	ui_fast_repeat_flush	pending_iters=1	771
89128943	worker	loop_interval_sleep	requested_ms=68	69418
89128948	worker	spike_loop_gap	after_loop=0 gap_us=69530	69530
89144943	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1065
89165793	worker	spike_loop_duration	iter=1 loop=1	36841
89166305	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	426
89166341	ui	session_end	finish success=yes loop=1
94456670	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
94462232	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	6133
94464076	worker	spike_loop_gap	after_loop=1 gap_us=5298284	5298284
94464926	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
94465697	ui	refreshWorkflowEditor	elapsed_ms=2	2000
94503853	worker	spike_loop_duration	iter=0	39772
94505492	ui	ui_fast_repeat_flush	pending_iters=1	653
94537520	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	918
94544790	ui	session_end	finish success=yes
94586947	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
94588698	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2146
94590322	worker	spike_loop_gap	after_loop=0 gap_us=86470	86470
94627136	worker	spike_loop_duration	iter=0	36811
94627886	ui	ui_fast_repeat_flush	pending_iters=1	452
94690281	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1068
94695904	ui	session_end	finish success=yes
94722427	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
94725980	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4027
94727556	worker	spike_loop_gap	after_loop=0 gap_us=100420	100420
94728064	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
94728502	ui	refreshWorkflowEditor	elapsed_ms=1	1000
94765597	worker	spike_loop_duration	iter=0	38036
94766752	ui	ui_fast_repeat_flush	pending_iters=1	913
94819630	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1031
94823084	ui	session_end	finish success=yes
94881243	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
94885793	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	5320
94888384	worker	spike_loop_gap	after_loop=0 gap_us=122784	122784
94936374	worker	spike_loop_duration	iter=0	47988
94937115	ui	ui_fast_repeat_flush	pending_iters=1	415
94977348	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1029
94977562	ui	session_end	finish success=yes
95011161	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
95013768	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3063
95015226	worker	spike_loop_gap	after_loop=0 gap_us=78851	78851
95051685	worker	spike_loop_duration	iter=0	36455
95052439	ui	ui_fast_repeat_flush	pending_iters=1	462
95111034	worker	loop_interval_sleep	requested_ms=58	59247
95111043	worker	spike_loop_gap	after_loop=0 gap_us=59358	59358
95139035	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1669
95148351	worker	spike_loop_duration	iter=1 loop=1	37306
95148770	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	347
95148933	ui	session_end	finish success=yes loop=1
95349362	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
95351949	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3112
95353423	worker	spike_loop_gap	after_loop=1 gap_us=205072	205072
95353950	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
95354350	ui	refreshWorkflowEditor	elapsed_ms=1	1000
95391025	worker	spike_loop_duration	iter=0	37601
95391802	ui	ui_fast_repeat_flush	pending_iters=1	470
95445977	worker	loop_interval_sleep	requested_ms=54	54886
95445984	worker	spike_loop_gap	after_loop=0 gap_us=54958	54958
95463141	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1111
95482228	worker	spike_loop_duration	iter=1 loop=1	36242
95486868	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	319
95486872	ui	session_end	finish success=yes loop=1
95519204	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
95521675	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2948
95523203	worker	spike_loop_gap	after_loop=1 gap_us=40974	40974
95562788	worker	spike_loop_duration	iter=0	39581
95563394	ui	ui_fast_repeat_flush	pending_iters=1	400
95566045	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	801
95573314	ui	session_end	finish success=yes
95626119	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
95629182	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3789
95630735	worker	spike_loop_gap	after_loop=0 gap_us=67946	67946
95636346	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
95636795	ui	refreshWorkflowEditor	elapsed_ms=1	1000
95670893	worker	spike_loop_duration	iter=0	40153
95671469	ui	ui_fast_repeat_flush	pending_iters=1	393
95746430	worker	loop_interval_sleep	requested_ms=74	75445
95746438	worker	spike_loop_gap	after_loop=0 gap_us=75546	75546
95764429	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1166
95786743	worker	spike_loop_duration	iter=1 loop=1	40303
95787336	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	294
95787339	ui	session_end	finish success=yes loop=1
95817083	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
95820184	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3960
95822087	worker	spike_loop_gap	after_loop=1 gap_us=35343	35343
95822612	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
95823011	ui	refreshWorkflowEditor	elapsed_ms=1	1000
95864869	worker	spike_loop_duration	iter=0	42781
95865928	ui	ui_fast_repeat_flush	pending_iters=1	382
95912792	ui	session_end	reason=preempted_by_new_session
95912846	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
95915761	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3472
95917896	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
95918351	ui	refreshWorkflowEditor	elapsed_ms=1	1000
95932339	worker	loop_interval_sleep	requested_ms=66	67403
95955599	worker	spike_loop_duration	iter=0 loop=1	38280
95956539	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	497
95969465	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1233
95977411	worker	spike_loop_duration	iter=1 loop=1	45063
95977604	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	62
95977945	ui	session_end	finish success=yes loop=1
96020265	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1351
96042084	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
96045372	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3794
96047278	worker	spike_loop_gap	after_loop=1 gap_us=69867	69867
96048703	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
96049042	ui	refreshWorkflowEditor	elapsed_ms=1	1000
96070766	ui	session_end	reason=preempted_by_new_session
96070822	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
96073639	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3292
96075880	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
96076308	ui	refreshWorkflowEditor	elapsed_ms=2	2000
96086563	worker	spike_loop_duration	iter=0	39283
96086884	ui	ui_fast_repeat_flush	pending_iters=1	9
96113449	worker	spike_loop_duration	iter=0	38193
96114493	ui	ui_fast_repeat_flush	pending_iters=1	353
96146136	worker	loop_interval_sleep	requested_ms=58	59528
96146143	worker	spike_loop_gap	after_loop=0 gap_us=32692	32692
96152623	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1374
96156167	ui	session_end	finish success=yes loop=1
96167994	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	955
96192313	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
96198117	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	6097
96198168	ui	ui_fast_repeat_flush	pending_iters=1	31
96198172	ui	session_end	finish success=yes
96217643	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
96218203	ui	refreshWorkflowEditor	elapsed_ms=1	1000
96287383	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	912
96327373	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
96330563	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3695
96334076	worker	spike_loop_gap	after_loop=0 gap_us=220620	220620
96372757	worker	spike_loop_duration	iter=0	38676
96373410	ui	ui_fast_repeat_flush	pending_iters=1	486
96431191	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1343
96436835	ui	session_end	finish success=yes
96467074	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
96469836	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3297
96471318	worker	spike_loop_gap	after_loop=0 gap_us=98561	98561
96510724	worker	spike_loop_duration	iter=0	39404
96511294	ui	ui_fast_repeat_flush	pending_iters=1	406
96567201	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1452
96572299	ui	session_end	finish success=yes
96627647	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
96630269	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3051
96631770	worker	spike_loop_gap	after_loop=0 gap_us=121045	121045
96671190	worker	spike_loop_duration	iter=0	39417
96671968	ui	ui_fast_repeat_flush	pending_iters=1	367
96744585	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1626
96750156	ui	session_end	finish success=yes
96820385	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
96822964	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3031
96824455	worker	spike_loop_gap	after_loop=0 gap_us=153264	153264
96864553	worker	spike_loop_duration	iter=0	40095
96865139	ui	ui_fast_repeat_flush	pending_iters=1	411
96917007	worker	loop_interval_sleep	requested_ms=51	52363
96917013	worker	spike_loop_gap	after_loop=0 gap_us=52460	52460
96919661	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1228
96953220	worker	spike_loop_duration	iter=1 loop=1	36204
96954018	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	734
96954062	ui	session_end	finish success=yes loop=1
97198814	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
97201915	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3576
97203600	worker	spike_loop_gap	after_loop=1 gap_us=250380	250380
97239968	worker	spike_loop_duration	iter=0	36363
97241026	ui	ui_fast_repeat_flush	pending_iters=1	502
97299076	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1328
97301987	ui	session_end	finish success=yes
97358693	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
97361188	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3228
97363129	worker	spike_loop_gap	after_loop=0 gap_us=123161	123161
97403317	worker	spike_loop_duration	iter=0	40184
97404416	ui	ui_fast_repeat_flush	pending_iters=1	503
97471767	worker	loop_interval_sleep	requested_ms=67	68197
97471774	worker	spike_loop_gap	after_loop=0 gap_us=68458	68458
97476798	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1711
97508576	worker	spike_loop_duration	iter=1 loop=1	36800
97509104	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	452
97509143	ui	session_end	finish success=yes loop=1
97525360	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
97528276	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3360
97529714	worker	spike_loop_gap	after_loop=1 gap_us=21136	21136
97578962	worker	spike_loop_duration	iter=0	49247
97579669	ui	ui_fast_repeat_flush	pending_iters=1	385
97644358	worker	loop_interval_sleep	requested_ms=64	65332
97644362	worker	spike_loop_gap	after_loop=0 gap_us=65399	65399
97949218	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	303095
97958803	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	52
97958807	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1
97980630	worker	spike_synthetic_key	dur_us=336250 loop=1	336250
97980646	worker	spike_loop_duration	iter=1 loop=1	336282
97981065	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	357
97981199	ui	session_end	finish success=yes loop=1
121732952	ui	switchToProfile.maybeSave	elapsed_ms=0	0
121733129	ui	switchToProfile.saveSettings	elapsed_ms=0	0
121733583	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
121733605	ui	switchToProfile.stopSessions	elapsed_ms=0	0
121734501	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
121734838	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
121735012	ui	refreshWorkflowEditor	elapsed_ms=0	0
121735243	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
121735416	ui	refreshWorkflowEditor	elapsed_ms=0	0
121735524	ui	syncHotkeys	elapsed_ms=0	0
121735590	ui	loadProjectFromFile	elapsed_ms=1	1000
121735619	ui	loadActiveProfile	elapsed_ms=1	1000
121735628	ui	switchToProfile.loadActiveProfile	elapsed_ms=1	1000
121737654	ui	switchToProfile	elapsed_ms=5	5000
121737675	ui	profile_switch	id=1f4a4367-387b-4984-9019-1b6fc9ad35d8 auto=yes	5064
121776482	ui	switchToProfile.maybeSave	elapsed_ms=0	0
121776624	ui	switchToProfile.saveSettings	elapsed_ms=0	0
121776857	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
121776874	ui	switchToProfile.stopSessions	elapsed_ms=0	0
121777795	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
121782397	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
121782727	ui	refreshWorkflowEditor	elapsed_ms=2	2000
121784242	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
121784686	ui	refreshWorkflowEditor	elapsed_ms=1	1000
121784811	ui	syncHotkeys	elapsed_ms=0	0
121784943	ui	loadProjectFromFile	elapsed_ms=7	7000
121784976	ui	loadActiveProfile	elapsed_ms=7	7000
121784987	ui	switchToProfile.loadActiveProfile	elapsed_ms=7	7000
121794233	ui	switchToProfile	elapsed_ms=18	18000
121794291	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	18287
121811917	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
121813396	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
121813884	ui	refreshWorkflowEditor	elapsed_ms=1	1000
121814532	ui	trigger_monitor_start	block=#1
121814744	ui	session_end	reason=preempted_by_new_session
121814829	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
121820054	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
121820916	ui	refreshWorkflowEditor	elapsed_ms=5	5000
121822848	ui	trigger_monitor_start	block=#1
123282498	ui	switchToProfile.maybeSave	elapsed_ms=0	0
123282647	ui	switchToProfile.saveSettings	elapsed_ms=0	0
123283130	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
123283163	ui	switchToProfile.stopSessions	elapsed_ms=0	0
123284531	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
123285040	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
123285351	ui	refreshWorkflowEditor	elapsed_ms=0	0
123285847	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
123286044	ui	refreshWorkflowEditor	elapsed_ms=0	0
123286121	ui	syncHotkeys	elapsed_ms=0	0
123286205	ui	loadProjectFromFile	elapsed_ms=2	2000
123286234	ui	loadActiveProfile	elapsed_ms=2	2000
123286243	ui	switchToProfile.loadActiveProfile	elapsed_ms=2	2000
123289523	worker	spike_loop_duration	iter=0	1448329
123289525	worker	spike_loop_duration	iter=0	1448367
123289762	ui	switchToProfile	elapsed_ms=7	7000
123289805	ui	profile_switch	id=1f4a4367-387b-4984-9019-1b6fc9ad35d8 auto=yes	7623
123702491	ui	switchToProfile.maybeSave	elapsed_ms=0	0
123702633	ui	switchToProfile.saveSettings	elapsed_ms=0	0
123702864	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
123702882	ui	switchToProfile.stopSessions	elapsed_ms=0	0
123703741	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
123712106	ui	WorkflowEditorPanel.refresh	elapsed_ms=4	4000
123712761	ui	refreshWorkflowEditor	elapsed_ms=5	5000
123716419	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
123716748	ui	refreshWorkflowEditor	elapsed_ms=3	3000
123716878	ui	syncHotkeys	elapsed_ms=0	0
123716990	ui	loadProjectFromFile	elapsed_ms=13	13000
123717021	ui	loadActiveProfile	elapsed_ms=13	13000
123717031	ui	switchToProfile.loadActiveProfile	elapsed_ms=13	13000
123721129	ui	switchToProfile	elapsed_ms=19	19000
123721167	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	19869
123734171	ui	session_end	reason=preempted_by_new_session
123734226	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
123735891	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
123736380	ui	refreshWorkflowEditor	elapsed_ms=1	1000
123737780	ui	trigger_monitor_start	block=#1
123738089	ui	session_end	reason=preempted_by_new_session
123738177	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
123743906	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
123745315	ui	refreshWorkflowEditor	elapsed_ms=6	6000
123747458	ui	trigger_monitor_start	block=#1
124124169	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	18
124124879	ui	session_end	reason=preempted_by_new_session
124125038	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
124127371	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
124128209	ui	refreshWorkflowEditor	elapsed_ms=2	2000
124175980	ui	user_input_interrupt	feature=aad48461-acdc-4484-a316-0637c4b4b8f3 vk=0x20
124179214	worker	spike_loop_duration	iter=0	44829
124179744	ui	ui_fast_repeat_flush	pending_iters=1	371
124179747	ui	session_end	finish success=no
125782630	ui	switchToProfile.maybeSave	elapsed_ms=0	0
125782782	ui	switchToProfile.saveSettings	elapsed_ms=0	0
125783059	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
125783086	ui	switchToProfile.stopSessions	elapsed_ms=0	0
125784110	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
125784452	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
125784609	ui	refreshWorkflowEditor	elapsed_ms=0	0
125784837	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
125784962	ui	refreshWorkflowEditor	elapsed_ms=0	0
125785043	ui	syncHotkeys	elapsed_ms=0	0
125785105	ui	loadProjectFromFile	elapsed_ms=1	1000
125785131	ui	loadActiveProfile	elapsed_ms=1	1000
125785140	ui	switchToProfile.loadActiveProfile	elapsed_ms=1	1000
125787086	ui	switchToProfile	elapsed_ms=4	4000
125787108	ui	profile_switch	id=1f4a4367-387b-4984-9019-1b6fc9ad35d8 auto=yes	4765
126269390	ui	switchToProfile.maybeSave	elapsed_ms=0	0
126269533	ui	switchToProfile.saveSettings	elapsed_ms=0	0
126269735	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
126269772	ui	switchToProfile.stopSessions	elapsed_ms=0	0
126270645	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
126275988	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
126276787	ui	refreshWorkflowEditor	elapsed_ms=3	3000
126280203	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
126280435	ui	refreshWorkflowEditor	elapsed_ms=3	3000
126280570	ui	syncHotkeys	elapsed_ms=0	0
126280691	ui	loadProjectFromFile	elapsed_ms=10	10000
126280725	ui	loadActiveProfile	elapsed_ms=10	10000
126280735	ui	switchToProfile.loadActiveProfile	elapsed_ms=10	10000
126283847	ui	switchToProfile	elapsed_ms=14	14000
126283912	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	15052
126296422	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
126299087	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
126299623	ui	refreshWorkflowEditor	elapsed_ms=2	2000
126300572	ui	trigger_monitor_start	block=#1
126300876	ui	session_end	reason=preempted_by_new_session
126300986	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
126305848	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
126306956	ui	refreshWorkflowEditor	elapsed_ms=5	5000
126308720	ui	trigger_monitor_start	block=#1
126650007	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	11
126650469	ui	session_end	reason=preempted_by_new_session
126650529	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
126652371	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
126652759	ui	refreshWorkflowEditor	elapsed_ms=1	1000
126689246	ui	user_input_interrupt	feature=aad48461-acdc-4484-a316-0637c4b4b8f3 vk=0x20
126698227	worker	spike_loop_duration	iter=0	41181
126698814	ui	ui_fast_repeat_flush	pending_iters=1	428
126698817	ui	session_end	finish success=no
127821287	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	9
127821864	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
127826712	worker	spike_loop_gap	after_loop=0 gap_us=1128482	1128482
127908739	worker	spike_loop_duration	iter=0	82022
127909494	ui	ui_fast_repeat_flush	pending_iters=1	520
127909691	ui	session_end	finish success=yes
129682610	ui	switchToProfile.maybeSave	elapsed_ms=0	0
129682762	ui	switchToProfile.saveSettings	elapsed_ms=0	0
129683018	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
129683034	ui	switchToProfile.stopSessions	elapsed_ms=0	0
129683917	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
129684226	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
129684379	ui	refreshWorkflowEditor	elapsed_ms=0	0
129684605	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
129684729	ui	refreshWorkflowEditor	elapsed_ms=0	0
129684807	ui	syncHotkeys	elapsed_ms=0	0
129684871	ui	loadProjectFromFile	elapsed_ms=1	1000
129684897	ui	loadActiveProfile	elapsed_ms=1	1000
129684906	ui	switchToProfile.loadActiveProfile	elapsed_ms=1	1000
129686935	ui	switchToProfile	elapsed_ms=4	4000
129686959	ui	profile_switch	id=1f4a4367-387b-4984-9019-1b6fc9ad35d8 auto=yes	4660
130180452	ui	switchToProfile.maybeSave	elapsed_ms=0	0
130180591	ui	switchToProfile.saveSettings	elapsed_ms=0	0
130180861	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
130180891	ui	switchToProfile.stopSessions	elapsed_ms=0	0
130181846	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
130186616	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
130186964	ui	refreshWorkflowEditor	elapsed_ms=2	2000
130188850	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
130189096	ui	refreshWorkflowEditor	elapsed_ms=2	2000
130189210	ui	syncHotkeys	elapsed_ms=0	0
130189336	ui	loadProjectFromFile	elapsed_ms=7	7000
130189368	ui	loadActiveProfile	elapsed_ms=8	8000
130189378	ui	switchToProfile.loadActiveProfile	elapsed_ms=8	8000
130193683	ui	switchToProfile	elapsed_ms=14	14000
130193726	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	14719
130210822	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
130211979	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
130212269	ui	refreshWorkflowEditor	elapsed_ms=1	1000
130212773	ui	trigger_monitor_start	block=#1
130212984	ui	session_end	reason=preempted_by_new_session
130213063	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
130217384	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
130218075	ui	refreshWorkflowEditor	elapsed_ms=4	4000
130220129	ui	trigger_monitor_start	block=#1
131097494	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	37
131097976	ui	session_end	reason=preempted_by_new_session
131098033	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
131099751	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
131100586	ui	refreshWorkflowEditor	elapsed_ms=2	2000
131144057	ui	user_input_interrupt	feature=aad48461-acdc-4484-a316-0637c4b4b8f3 vk=0x20
131148115	worker	spike_loop_duration	iter=0	42018
131148904	ui	ui_fast_repeat_flush	pending_iters=1	422
131148908	ui	session_end	finish success=no
132014282	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	21
132014850	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
132020150	worker	spike_loop_gap	after_loop=0 gap_us=872031	872031
132103230	worker	spike_loop_duration	iter=0	83076
132103832	ui	ui_fast_repeat_flush	pending_iters=1	410
132104011	ui	session_end	finish success=yes
133700306	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	21
133700953	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
133706602	worker	spike_loop_gap	after_loop=0 gap_us=1603372	1603372
133788933	worker	spike_loop_duration	iter=0	82327
133789537	ui	ui_fast_repeat_flush	pending_iters=1	406
133789713	ui	session_end	finish success=yes
134700542	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	11
134701032	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
134705866	worker	spike_loop_gap	after_loop=0 gap_us=916933	916933
134788571	worker	spike_loop_duration	iter=0	82701
134789168	ui	ui_fast_repeat_flush	pending_iters=1	359
134789343	ui	session_end	finish success=yes
145320510	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
145323224	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3216
145324752	worker	spike_loop_gap	after_loop=0 gap_us=10536180	10536180
145325288	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
145325870	ui	refreshWorkflowEditor	elapsed_ms=2	2000
145363426	worker	spike_loop_duration	iter=0	38671
145364098	ui	ui_fast_repeat_flush	pending_iters=1	501
145407812	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1365
145414711	ui	session_end	finish success=yes
145495498	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
145497963	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3017
145499849	worker	spike_loop_gap	after_loop=0 gap_us=136424	136424
145537203	worker	spike_loop_duration	iter=0	37352
145538421	ui	ui_fast_repeat_flush	pending_iters=1	427
145569171	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1052
145578212	ui	session_end	finish success=yes
145608367	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
145611028	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3196
145612936	worker	spike_loop_gap	after_loop=0 gap_us=75731	75731
145652602	worker	spike_loop_duration	iter=0	39662
145653273	ui	ui_fast_repeat_flush	pending_iters=1	469
145725016	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1297
145736219	ui	session_end	finish success=yes
145756051	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
145758971	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3688
145760757	worker	spike_loop_gap	after_loop=0 gap_us=108155	108155
145761315	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
145761729	ui	refreshWorkflowEditor	elapsed_ms=2	2000
145802927	worker	spike_loop_duration	iter=0	42167
145803678	ui	ui_fast_repeat_flush	pending_iters=1	413
145864084	worker	loop_interval_sleep	requested_ms=60	61060
145864091	worker	spike_loop_gap	after_loop=0 gap_us=61165	61165
145879607	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1368
145900963	worker	spike_loop_duration	iter=1 loop=1	36869
145901404	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	358
145901437	ui	session_end	finish success=yes loop=1
145937604	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
145939844	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2633
145942272	worker	spike_loop_gap	after_loop=1 gap_us=41309	41309
145943112	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
145943527	ui	refreshWorkflowEditor	elapsed_ms=2	2000
145981138	worker	spike_loop_duration	iter=0	38864
145982513	ui	ui_fast_repeat_flush	pending_iters=1	783
146037434	worker	loop_interval_sleep	requested_ms=55	56191
146037438	worker	spike_loop_gap	after_loop=0 gap_us=56300	56300
146080707	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1384
146087564	worker	spike_loop_duration	iter=1 loop=1	50124
146088307	ui	session_end	reason=preempted_by_new_session loop=1
146088358	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
146090833	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2893
146091244	ui	ui_fast_repeat_flush	pending_iters=1	395
146091248	ui	session_end	finish success=yes
146110384	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
146110807	ui	refreshWorkflowEditor	elapsed_ms=1	1000
146167450	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
146170774	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	4066
146172662	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
146173087	ui	refreshWorkflowEditor	elapsed_ms=1	1000
146203149	worker	loop_interval_sleep	requested_ms=57	58398
146222695	worker	spike_loop_duration	iter=0 loop=1	50323
146224258	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1089
146224658	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	313
146239400	worker	spike_loop_duration	iter=1 loop=1	36243
146239620	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	35
146239625	ui	session_end	finish success=yes loop=1
146271857	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1125
146301712	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
146304519	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3296
146306059	worker	spike_loop_gap	after_loop=1 gap_us=66658	66658
146309467	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
146309902	ui	refreshWorkflowEditor	elapsed_ms=1	1000
146337052	ui	session_end	reason=preempted_by_new_session
146337104	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
146340687	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3909
146345015	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
146345666	ui	refreshWorkflowEditor	elapsed_ms=2	2000
146346831	worker	spike_loop_duration	iter=0	40770
146349824	ui	ui_fast_repeat_flush	pending_iters=1	10
146380127	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1579
146390752	worker	spike_loop_duration	iter=0	46743
146391962	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	913
146392385	ui	ui_fast_repeat_flush	pending_iters=1	309
146392388	ui	session_end	finish success=yes
146460201	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
146462396	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2683
146464109	worker	spike_loop_gap	after_loop=0 gap_us=73355	73355
146467456	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
146467879	ui	refreshWorkflowEditor	elapsed_ms=1	1000
146468723	ui	session_end	reason=preempted_by_new_session
146468772	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
146471104	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2669
146473245	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
146473707	ui	refreshWorkflowEditor	elapsed_ms=1	1000
146502173	worker	spike_loop_duration	iter=0	38043
146502477	ui	ui_fast_repeat_flush	pending_iters=1	25
146515399	worker	spike_loop_duration	iter=0	42728
146516070	ui	ui_fast_repeat_flush	pending_iters=1	404
146546324	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1298
146546507	ui	session_end	finish success=yes
146568327	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1216
146589692	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
146591700	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2336
146593169	worker	spike_loop_gap	after_loop=0 gap_us=77771	77771
146595354	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
146595749	ui	refreshWorkflowEditor	elapsed_ms=1	1000
146609873	ui	session_end	reason=preempted_by_new_session
146609926	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
146612817	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3483
146615098	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
146615468	ui	refreshWorkflowEditor	elapsed_ms=2	2000
146630084	worker	spike_loop_duration	iter=0	36910
146630322	ui	ui_fast_repeat_flush	pending_iters=1	23
146662775	worker	spike_loop_duration	iter=0	48463
146664306	ui	session_end	reason=preempted_by_new_session
146664358	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
146666726	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3396
146667756	ui	session_end	finish success=yes
146681164	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	14433
146984760	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	303592
147001211	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
147001718	ui	refreshWorkflowEditor	elapsed_ms=1	1000
147010557	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1492
147010623	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	63
147010626	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2
147010667	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	40
147016864	worker	spike_synthetic_key	dur_us=337162	337162
147028977	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	6
147034128	worker	spike_synthetic_key	dur_us=339575	339575
147069979	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
147072544	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2976
147115006	worker	spike_loop_duration	iter=0	39921
147115581	ui	ui_fast_repeat_flush	pending_iters=1	405
147166239	worker	loop_interval_sleep	requested_ms=50	51143
147166246	worker	spike_loop_gap	after_loop=0 gap_us=51242	51242
147192639	ui	session_end	reason=preempted_by_new_session loop=1
147192692	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
147195210	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3147
147197360	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
147197743	ui	refreshWorkflowEditor	elapsed_ms=2	2000
147200797	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1053
147204876	worker	spike_loop_duration	iter=1	38628
147205262	ui	ui_fast_repeat_flush	pending_iters=1	32
147205267	ui	session_end	finish success=yes
147281871	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
147284615	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3386
147286133	worker	spike_loop_gap	after_loop=1 gap_us=81255	81255
147287380	worker	loop_interval_sleep	requested_ms=50	50827
147287383	worker	spike_loop_gap	after_loop=1 gap_us=82506	82506
147290349	ui	WorkflowEditorPanel.refresh	elapsed_ms=0 loop=1	0
147290760	ui	refreshWorkflowEditor	elapsed_ms=1 loop=1	1000
147300985	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	750
147329943	worker	spike_loop_duration	iter=0 loop=1	43804
147330502	worker	spike_loop_duration	iter=1 loop=1	43117
147331202	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	392
147331215	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	9
147331219	ui	session_end	finish success=yes loop=1
147371150	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
147373165	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2339
147374940	worker	spike_loop_gap	after_loop=1 gap_us=44436	44436
147382275	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
147382678	ui	refreshWorkflowEditor	elapsed_ms=1	1000
147390949	ui	session_end	reason=preempted_by_new_session
147391006	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
147393463	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2950
147395819	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
147396186	ui	refreshWorkflowEditor	elapsed_ms=0	0
147397459	worker	loop_interval_sleep	requested_ms=66	67454
147415613	worker	spike_loop_duration	iter=0 loop=1	40668
147415770	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	25
147417410	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	940
147436895	worker	spike_loop_duration	iter=0 loop=1	41618
147437707	worker	spike_loop_duration	iter=1 loop=1	40244
147438331	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	481
147438337	ui	session_end	finish success=yes loop=1
147475755	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	798
147476571	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	814
147524385	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
147527502	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3668
147529058	worker	spike_loop_gap	after_loop=1 gap_us=91350	91350
147529525	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
147529944	ui	refreshWorkflowEditor	elapsed_ms=1	1000
147542165	ui	session_end	reason=preempted_by_new_session
147542220	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
147544555	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2764
147546719	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
147547119	ui	refreshWorkflowEditor	elapsed_ms=1	1000
147566998	worker	spike_loop_duration	iter=0	37936
147567190	ui	ui_fast_repeat_flush	pending_iters=1	15
147598105	worker	spike_loop_duration	iter=0	51599
147602394	ui	ui_fast_repeat_flush	pending_iters=1	418
147620037	worker	loop_interval_sleep	requested_ms=52	52935
147620045	worker	spike_loop_gap	after_loop=0 gap_us=21938	21938
147647416	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1179
147650256	ui	session_end	finish success=yes loop=1
147684740	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2245
187726229	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
187728996	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3342
187731028	worker	spike_loop_gap	after_loop=0 gap_us=40132921	40132921
187771525	worker	spike_loop_duration	iter=0	40493
187772359	ui	ui_fast_repeat_flush	pending_iters=1	423
187845071	worker	loop_interval_sleep	requested_ms=72	73452
187845078	worker	spike_loop_gap	after_loop=0 gap_us=73555	73555
187874759	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1303
187886290	worker	spike_loop_duration	iter=1 loop=1	41209
187887015	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	307
187887018	ui	session_end	finish success=yes loop=1
188599754	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
188602377	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3106
188603920	worker	spike_loop_gap	after_loop=1 gap_us=717628	717628
188604772	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
188605188	ui	refreshWorkflowEditor	elapsed_ms=2	2000
188642273	worker	spike_loop_duration	iter=0	38351
188642859	ui	ui_fast_repeat_flush	pending_iters=1	418
188703613	worker	loop_interval_sleep	requested_ms=60	61255
188703620	worker	spike_loop_gap	after_loop=0 gap_us=61347	61347
188736923	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	990
188740118	worker	spike_loop_duration	iter=1 loop=1	36496
188740836	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	494
188740840	ui	session_end	finish success=yes loop=1
189903754	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
189906155	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2925
189907849	worker	spike_loop_gap	after_loop=1 gap_us=1167730	1167730
189908710	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
189909079	ui	refreshWorkflowEditor	elapsed_ms=2	2000
189947941	worker	spike_loop_duration	iter=0	40088
189948466	ui	ui_fast_repeat_flush	pending_iters=1	387
189998288	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1449
190001500	ui	session_end	finish success=yes
190068455	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
190070636	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2655
190072400	worker	spike_loop_gap	after_loop=0 gap_us=124457	124457
190109750	worker	spike_loop_duration	iter=0	37346
190110327	ui	ui_fast_repeat_flush	pending_iters=1	408
190165003	worker	loop_interval_sleep	requested_ms=54	55159
190165011	worker	spike_loop_gap	after_loop=0 gap_us=55263	55263
190168958	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1383
190201387	worker	spike_loop_duration	iter=1 loop=1	36373
190201819	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	375
190201991	ui	session_end	finish success=yes loop=1
190220240	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
190223925	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4157
190225629	worker	spike_loop_gap	after_loop=1 gap_us=24240	24240
190269218	worker	spike_loop_duration	iter=0	43587
190270225	ui	ui_fast_repeat_flush	pending_iters=1	415
190318690	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	991
190321215	ui	session_end	finish success=yes
190636164	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
190638004	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2187
190639789	worker	spike_loop_gap	after_loop=0 gap_us=370567	370567
190640278	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
190640702	ui	refreshWorkflowEditor	elapsed_ms=2	2000
190677578	worker	spike_loop_duration	iter=0	37786
190678248	ui	ui_fast_repeat_flush	pending_iters=1	440
190756770	ui	session_end	reason=preempted_by_new_session
190756825	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
190757357	worker	loop_interval_sleep	requested_ms=78	79692
190759912	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	3648
190762846	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
190763258	ui	refreshWorkflowEditor	elapsed_ms=1	1000
190787472	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1024
190796608	worker	spike_loop_duration	iter=1	39247
190796867	ui	ui_fast_repeat_flush	pending_iters=1	38
190797076	ui	session_end	finish success=yes
190862760	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1148
190873030	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
190876613	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4157
190878025	worker	spike_loop_gap	after_loop=1 gap_us=81415	81415
190878663	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
190879092	ui	refreshWorkflowEditor	elapsed_ms=1	1000
190904037	ui	session_end	reason=preempted_by_new_session
190904091	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
190906722	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3105
190908755	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
190909147	ui	refreshWorkflowEditor	elapsed_ms=2	2000
190920187	worker	spike_loop_duration	iter=0	42157
190920497	ui	ui_fast_repeat_flush	pending_iters=1	16
190959531	worker	spike_loop_duration	iter=0	51160
190960304	ui	ui_fast_repeat_flush	pending_iters=1	395
190970419	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1162
190974852	ui	session_end	finish success=yes
191041183	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	971
191045956	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
191049134	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3656
191050612	worker	spike_loop_gap	after_loop=0 gap_us=91078	91078
191055070	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
191055472	ui	refreshWorkflowEditor	elapsed_ms=1	1000
191069507	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	84
191071911	worker	spike_loop_duration	iter=1	37749
191072019	ui	ui_fast_repeat_flush	pending_iters=1	39
191072332	ui	session_end	finish success=yes
191156940	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1428
191201129	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
191203573	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	6115
191205258	worker	spike_loop_gap	after_loop=1 gap_us=133344	133344
191241771	worker	spike_loop_duration	iter=0	36509
191242481	ui	ui_fast_repeat_flush	pending_iters=1	422
191247233	ui	session_end	reason=preempted_by_new_session
191247290	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
191249766	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2934
191252009	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
191252389	ui	refreshWorkflowEditor	elapsed_ms=2	2000
191278895	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	941
191286037	ui	session_end	finish success=yes
191299808	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3
191414212	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
191417224	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3735
191419327	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
191419752	ui	refreshWorkflowEditor	elapsed_ms=1	1000
191456588	worker	spike_loop_duration	iter=0	37816
191457475	ui	ui_fast_repeat_flush	pending_iters=1	518
191478227	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1223
191479703	worker	loop_interval_sleep	requested_ms=80	81588
191479708	worker	spike_loop_gap	after_loop=0 gap_us=23122	23122
191492425	ui	session_end	finish success=yes loop=2
191514708	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1256
191515227	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
191517883	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3021
191519760	worker	spike_loop_gap	after_loop=0 gap_us=63172	63172
191522422	worker	spike_loop_duration	iter=2	42712
191524334	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
191524770	ui	refreshWorkflowEditor	elapsed_ms=1	1000
191525327	ui	ui_fast_repeat_flush	pending_iters=1	0
191525330	ui	session_end	finish success=yes
191543899	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
191546164	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2618
191547721	worker	spike_loop_gap	after_loop=2 gap_us=25296	25296
191553298	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
191553724	ui	refreshWorkflowEditor	elapsed_ms=1	1000
191559118	worker	spike_loop_duration	iter=0	39356
191559401	ui	ui_fast_repeat_flush	pending_iters=1	9
191563840	ui	session_end	reason=preempted_by_new_session
191563908	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
191566198	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2738
191568354	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
191568744	ui	refreshWorkflowEditor	elapsed_ms=2	2000
191598905	worker	spike_loop_duration	iter=0	51182
191603554	ui	ui_fast_repeat_flush	pending_iters=1	9
191607837	worker	spike_loop_duration	iter=0	40051
191608527	ui	ui_fast_repeat_flush	pending_iters=1	364
191626573	worker	loop_interval_sleep	requested_ms=66	67393
191626582	worker	spike_loop_gap	after_loop=0 gap_us=18744	18744
191641897	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1146
191652621	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1123
191674453	worker	loop_interval_sleep	requested_ms=65 loop=1	66546
191674459	worker	spike_loop_gap	after_loop=0 gap_us=66621 loop=1	66621
191677930	worker	spike_loop_duration	iter=1 loop=1	51345
191679320	ui	session_end	finish success=yes loop=1
191712239	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1203
194170869	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	10
194171335	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
194173283	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
194173748	ui	refreshWorkflowEditor	elapsed_ms=2	2000
194178703	worker	spike_loop_gap	after_loop=1 gap_us=2500772	2500772
194266827	worker	spike_loop_duration	iter=0	88121
194267546	ui	ui_fast_repeat_flush	pending_iters=1	439
194267720	ui	session_end	finish success=yes
195655087	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
195657976	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3442
195660301	worker	spike_loop_gap	after_loop=0 gap_us=1393473	1393473
195660626	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
195661040	ui	refreshWorkflowEditor	elapsed_ms=2	2000
195697998	worker	spike_loop_duration	iter=0	37693
195698909	ui	ui_fast_repeat_flush	pending_iters=1	651
195754945	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1690
195759451	ui	session_end	finish success=yes
195814666	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
195817210	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3079
195818680	worker	spike_loop_gap	after_loop=0 gap_us=120683	120683
195819190	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
195819653	ui	refreshWorkflowEditor	elapsed_ms=2	2000
195860500	worker	spike_loop_duration	iter=0	41815
195861124	ui	ui_fast_repeat_flush	pending_iters=1	421
195901552	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	915
195906358	ui	session_end	finish success=yes
197621253	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
197624125	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3393
197625628	worker	spike_loop_gap	after_loop=0 gap_us=1765131	1765131
197626051	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
197626486	ui	refreshWorkflowEditor	elapsed_ms=1	1000
197664023	worker	spike_loop_duration	iter=0	38390
197664891	ui	ui_fast_repeat_flush	pending_iters=1	586
197699518	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1035
197705135	ui	session_end	finish success=yes
201532843	ui	switchToProfile.maybeSave	elapsed_ms=0	0
201533016	ui	switchToProfile.saveSettings	elapsed_ms=0	0
201533412	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
201533439	ui	switchToProfile.stopSessions	elapsed_ms=0	0
201534886	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
201535257	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
201535496	ui	refreshWorkflowEditor	elapsed_ms=0	0
201535765	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
201535926	ui	refreshWorkflowEditor	elapsed_ms=0	0
201536039	ui	syncHotkeys	elapsed_ms=0	0
201536113	ui	loadProjectFromFile	elapsed_ms=2	2000
201536142	ui	loadActiveProfile	elapsed_ms=2	2000
201536151	ui	switchToProfile.loadActiveProfile	elapsed_ms=2	2000
201538161	ui	switchToProfile	elapsed_ms=5	5000
201538180	ui	profile_switch	id=1f4a4367-387b-4984-9019-1b6fc9ad35d8 auto=yes	5680
204682613	ui	switchToProfile.maybeSave	elapsed_ms=0	0
204682748	ui	switchToProfile.saveSettings	elapsed_ms=0	0
204683041	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
204683074	ui	switchToProfile.stopSessions	elapsed_ms=0	0
204683981	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
204685501	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
204685724	ui	refreshWorkflowEditor	elapsed_ms=1	1000
204686315	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
204686471	ui	refreshWorkflowEditor	elapsed_ms=0	0
204686556	ui	syncHotkeys	elapsed_ms=0	0
204686644	ui	loadProjectFromFile	elapsed_ms=2	2000
204686673	ui	loadActiveProfile	elapsed_ms=3	3000
204686681	ui	switchToProfile.loadActiveProfile	elapsed_ms=3	3000
204689054	ui	switchToProfile	elapsed_ms=6	6000
204689096	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	6845
204691693	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
204691989	ui	refreshWorkflowEditor	elapsed_ms=1	1000
204696262	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
204697451	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
204697941	ui	refreshWorkflowEditor	elapsed_ms=1	1000
204698581	ui	trigger_monitor_start	block=#1
204698791	ui	session_end	reason=preempted_by_new_session
204698879	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
204704498	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
204705151	ui	refreshWorkflowEditor	elapsed_ms=5	5000
204707230	ui	trigger_monitor_start	block=#1
204842084	worker	imagefind_poll	#1 poll=1 matched=no conf=0.294 cap_us=111868 match_us=518	112386
204877536	worker	imagefind_poll	#1 poll=1 matched=no conf=0.486 cap_us=107658 match_us=40064	147722
205228560	worker	imagefind_poll	#1 poll=2 matched=no conf=0.313 cap_us=3673 match_us=39644	43317
205579448	worker	imagefind_poll	#1 poll=3 matched=no conf=0.257 cap_us=4060 match_us=39479	43539
205930076	worker	imagefind_poll	#1 poll=4 matched=no conf=0.257 cap_us=4175 match_us=39401	43576
206281166	worker	imagefind_poll	#1 poll=5 matched=no conf=0.242 cap_us=4004 match_us=40139	44143
206632152	worker	imagefind_poll	#1 poll=6 matched=no conf=0.189 cap_us=3798 match_us=39561	43359
206985467	worker	imagefind_poll	#1 poll=7 matched=no conf=0.189 cap_us=4288 match_us=41517	45805
207339890	worker	imagefind_poll	#1 poll=8 matched=no conf=0.424 cap_us=3921 match_us=42921	46842
207690781	worker	imagefind_poll	#1 poll=9 matched=no conf=0.466 cap_us=3963 match_us=39304	43267
208041087	worker	imagefind_poll	#1 poll=10 matched=no conf=0.472 cap_us=3979 match_us=39387	43366
208279816	worker	imagefind_poll	#1 poll=12 matched=yes conf=1.000 cap_us=3696 match_us=542	4238
208279880	worker	spike_loop_duration	iter=0	3570472
208283520	ui	trigger_action_start	feature=올리폿 block=#1
210329318	worker	synthetic_mouse_click	synthetic=1 path=screen x=1405 y=822	31307
210381501	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=871	52121
210421059	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=923	39524
210472960	worker	synthetic_mouse_click	synthetic=1 path=screen x=1641 y=1229	51839
210512208	worker	synthetic_mouse_click	synthetic=1 path=screen x=1989 y=1419	39164
210553143	worker	synthetic_mouse_click	synthetic=1 path=screen x=1405 y=877	40893
210588009	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=871	34821
210630687	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=923	42637
210663865	worker	synthetic_mouse_click	synthetic=1 path=screen x=1641 y=1229	33139
210708594	worker	synthetic_mouse_click	synthetic=1 path=screen x=1989 y=1419	44686
210745123	worker	synthetic_mouse_click	synthetic=1 path=screen x=1401 y=921	36485
210787885	worker	synthetic_mouse_click	synthetic=1 path=screen x=1644 y=870	42711
210821122	worker	synthetic_mouse_click	synthetic=1 path=screen x=1641 y=926	33201
210866600	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=1227	45428
210904126	worker	synthetic_mouse_click	synthetic=1 path=screen x=1987 y=1418	37478
210944478	worker	synthetic_mouse_click	synthetic=1 path=screen x=1402 y=968	40291
210978067	worker	synthetic_mouse_click	synthetic=1 path=screen x=1643 y=871	33540
211022289	worker	synthetic_mouse_click	synthetic=1 path=screen x=1636 y=924	44175
211060960	worker	synthetic_mouse_click	synthetic=1 path=screen x=1642 y=1224	38433
211114304	worker	synthetic_mouse_click	synthetic=1 path=screen x=1975 y=1420	53295
211154490	worker	synthetic_mouse_click	synthetic=1 path=screen x=1396 y=1024	40107
211208146	worker	synthetic_mouse_click	synthetic=1 path=screen x=1636 y=874	53621
211245399	worker	synthetic_mouse_click	synthetic=1 path=screen x=1636 y=925	37213
211287236	worker	synthetic_mouse_click	synthetic=1 path=screen x=1643 y=1226	41797
211320769	worker	synthetic_mouse_click	synthetic=1 path=screen x=1973 y=1418	33495
211368261	worker	synthetic_mouse_click	synthetic=1 path=screen x=1408 y=1114	47446
211404441	worker	synthetic_mouse_click	synthetic=1 path=screen x=1642 y=874	36136
211445756	worker	synthetic_mouse_click	synthetic=1 path=screen x=1643 y=919	41253
211478662	worker	synthetic_mouse_click	synthetic=1 path=screen x=1642 y=1226	32855
211523952	worker	synthetic_mouse_click	synthetic=1 path=screen x=1968 y=1427	45234
211562322	worker	synthetic_mouse_click	synthetic=1 path=screen x=1405 y=1166	38325
211601891	worker	synthetic_mouse_click	synthetic=1 path=screen x=1643 y=873	39528
211636539	worker	synthetic_mouse_click	synthetic=1 path=screen x=1644 y=933	34611
211678508	worker	synthetic_mouse_click	synthetic=1 path=screen x=1643 y=1224	41941
211713664	worker	synthetic_mouse_click	synthetic=1 path=screen x=1967 y=1427	35101
211760120	worker	synthetic_mouse_click	synthetic=1 path=screen x=1402 y=1224	46405
211796992	worker	synthetic_mouse_click	synthetic=1 path=screen x=1637 y=876	36838
211848781	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=924	51725
211889092	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=1228	40269
211942304	worker	synthetic_mouse_click	synthetic=1 path=screen x=1970 y=1419	53178
211979253	worker	synthetic_mouse_click	synthetic=1 path=screen x=1401 y=1271	36913
212024052	worker	synthetic_mouse_click	synthetic=1 path=screen x=1639 y=871	44749
212060618	worker	synthetic_mouse_click	synthetic=1 path=screen x=1641 y=922	36502
212102577	worker	synthetic_mouse_click	synthetic=1 path=screen x=1643 y=1228	41911
212136614	worker	synthetic_mouse_click	synthetic=1 path=screen x=1975 y=1417	33990
212180740	worker	synthetic_mouse_click	synthetic=1 path=screen x=1405 y=1325	44071
212223274	worker	synthetic_mouse_click	synthetic=1 path=screen x=1641 y=871	42475
212272356	worker	synthetic_mouse_click	synthetic=1 path=screen x=1644 y=927	49013
212311827	worker	synthetic_mouse_click	synthetic=1 path=screen x=1643 y=1226	39436
212353282	worker	synthetic_mouse_click	synthetic=1 path=screen x=1975 y=1423	41402
212353313	worker	spike_loop_duration	iter=0	4069146
212359743	ui	trigger_cooldown_start	ms=30000
212476311	worker	imagefind_poll	#1 poll=11 matched=no conf=0.405 cap_us=3888 match_us=39168	43056
212830190	worker	imagefind_poll	#1 poll=12 matched=no conf=0.402 cap_us=3647 match_us=41651	45298
213184482	worker	imagefind_poll	#1 poll=13 matched=no conf=0.405 cap_us=6531 match_us=39681	46212
213541267	worker	imagefind_poll	#1 poll=14 matched=no conf=0.453 cap_us=4191 match_us=43376	47567
213895681	worker	imagefind_poll	#1 poll=15 matched=no conf=0.405 cap_us=4249 match_us=41765	46014
214249508	worker	imagefind_poll	#1 poll=16 matched=no conf=0.498 cap_us=4377 match_us=41670	46047
214601984	worker	imagefind_poll	#1 poll=17 matched=no conf=0.452 cap_us=3732 match_us=39901	43633
214953160	worker	imagefind_poll	#1 poll=18 matched=no conf=0.445 cap_us=3738 match_us=39725	43463
215303094	worker	imagefind_poll	#1 poll=19 matched=no conf=0.452 cap_us=3689 match_us=39516	43205
215653657	worker	imagefind_poll	#1 poll=20 matched=no conf=0.445 cap_us=3728 match_us=39546	43274
216006107	worker	imagefind_poll	#1 poll=21 matched=no conf=0.452 cap_us=4351 match_us=39912	44263
216360265	worker	imagefind_poll	#1 poll=22 matched=no conf=0.445 cap_us=3903 match_us=41013	44916
216712470	worker	imagefind_poll	#1 poll=23 matched=no conf=0.452 cap_us=4004 match_us=39369	43373
217064825	worker	imagefind_poll	#1 poll=24 matched=no conf=0.445 cap_us=4125 match_us=40053	44178
217418041	worker	imagefind_poll	#1 poll=25 matched=no conf=0.452 cap_us=4111 match_us=39835	43946
217769026	worker	imagefind_poll	#1 poll=26 matched=no conf=0.445 cap_us=3644 match_us=39470	43114
218119498	worker	imagefind_poll	#1 poll=27 matched=no conf=0.452 cap_us=3626 match_us=39470	43096
218472010	worker	imagefind_poll	#1 poll=28 matched=no conf=0.445 cap_us=4487 match_us=40284	44771
218823420	worker	imagefind_poll	#1 poll=29 matched=no conf=0.424 cap_us=4128 match_us=39750	43878
219175798	worker	imagefind_poll	#1 poll=30 matched=no conf=0.439 cap_us=4525 match_us=39531	44056
219525862	worker	imagefind_poll	#1 poll=31 matched=no conf=0.424 cap_us=4257 match_us=39142	43399
219876070	worker	imagefind_poll	#1 poll=32 matched=no conf=0.426 cap_us=3771 match_us=39213	42984
220228075	worker	imagefind_poll	#1 poll=33 matched=no conf=0.426 cap_us=3677 match_us=39434	43111
220580490	worker	imagefind_poll	#1 poll=34 matched=no conf=0.426 cap_us=3913 match_us=39965	43878
220933933	worker	imagefind_poll	#1 poll=35 matched=no conf=0.426 cap_us=4430 match_us=39811	44241
221286644	worker	imagefind_poll	#1 poll=36 matched=no conf=0.426 cap_us=3700 match_us=39878	43578
221637700	worker	imagefind_poll	#1 poll=37 matched=no conf=0.426 cap_us=3958 match_us=39186	43144
221949417	worker	capture_imagefind	area=2	4132
221989687	worker	imagefind_poll	#1 poll=38 matched=no conf=0.426 cap_us=4208 match_us=40064	44272
222346485	worker	imagefind_poll	#1 poll=39 matched=no conf=0.452 cap_us=3878 match_us=40637	44515
222697577	worker	imagefind_poll	#1 poll=40 matched=no conf=0.445 cap_us=4355 match_us=39418	43773
223050103	worker	imagefind_poll	#1 poll=41 matched=no conf=0.452 cap_us=3846 match_us=39454	43300
223400670	worker	imagefind_poll	#1 poll=42 matched=no conf=0.445 cap_us=3871 match_us=39369	43240
223752143	worker	imagefind_poll	#1 poll=43 matched=no conf=0.452 cap_us=3910 match_us=39495	43405
224102956	worker	imagefind_poll	#1 poll=44 matched=no conf=0.445 cap_us=3706 match_us=39378	43084
224455283	worker	imagefind_poll	#1 poll=45 matched=no conf=0.452 cap_us=4444 match_us=39712	44156
224809919	worker	imagefind_poll	#1 poll=46 matched=no conf=0.445 cap_us=5026 match_us=41174	46200
225160632	worker	imagefind_poll	#1 poll=47 matched=no conf=0.452 cap_us=3653 match_us=39385	43038
225512023	worker	imagefind_poll	#1 poll=48 matched=no conf=0.445 cap_us=4242 match_us=39868	44110
225863689	worker	imagefind_poll	#1 poll=49 matched=no conf=0.452 cap_us=4446 match_us=39696	44142
226217828	worker	imagefind_poll	#1 poll=50 matched=no conf=0.445 cap_us=6657 match_us=39781	46438
226569381	worker	imagefind_poll	#1 poll=51 matched=no conf=0.452 cap_us=3711 match_us=39785	43496
226919986	worker	imagefind_poll	#1 poll=52 matched=no conf=0.445 cap_us=4044 match_us=39373	43417
227273390	worker	imagefind_poll	#1 poll=53 matched=no conf=0.452 cap_us=4783 match_us=39937	44720
227624436	worker	imagefind_poll	#1 poll=54 matched=no conf=0.445 cap_us=3924 match_us=39347	43271
227974532	worker	imagefind_poll	#1 poll=55 matched=no conf=0.452 cap_us=3617 match_us=39516	43133
228325467	worker	imagefind_poll	#1 poll=56 matched=no conf=0.445 cap_us=3928 match_us=39473	43401
228676296	worker	imagefind_poll	#1 poll=57 matched=no conf=0.452 cap_us=4155 match_us=39533	43688
229026635	worker	imagefind_poll	#1 poll=58 matched=no conf=0.445 cap_us=3626 match_us=39719	43345
229382723	worker	imagefind_poll	#1 poll=59 matched=no conf=0.452 cap_us=4224 match_us=42483	46707
229733977	worker	imagefind_poll	#1 poll=60 matched=no conf=0.445 cap_us=3916 match_us=39784	43700
230084723	worker	imagefind_poll	#1 poll=61 matched=no conf=0.452 cap_us=3649 match_us=39676	43325
230434955	worker	imagefind_poll	#1 poll=62 matched=no conf=0.445 cap_us=3622 match_us=39433	43055
230784997	worker	imagefind_poll	#1 poll=63 matched=no conf=0.452 cap_us=3639 match_us=39160	42799
231136184	worker	imagefind_poll	#1 poll=64 matched=no conf=0.445 cap_us=4035 match_us=39522	43557
231489401	worker	imagefind_poll	#1 poll=65 matched=no conf=0.452 cap_us=4576 match_us=39245	43821
231846302	worker	imagefind_poll	#1 poll=66 matched=no conf=0.445 cap_us=4397 match_us=45220	49617
232198308	worker	imagefind_poll	#1 poll=67 matched=no conf=0.452 cap_us=4208 match_us=39901	44109
232551711	worker	imagefind_poll	#1 poll=68 matched=no conf=0.445 cap_us=3741 match_us=39351	43092
232901775	worker	imagefind_poll	#1 poll=69 matched=no conf=0.452 cap_us=3708 match_us=39182	42890
233252966	worker	imagefind_poll	#1 poll=70 matched=no conf=0.445 cap_us=3874 match_us=39565	43439
233604162	worker	imagefind_poll	#1 poll=71 matched=no conf=0.452 cap_us=4035 match_us=39737	43772
233955417	worker	imagefind_poll	#1 poll=72 matched=no conf=0.445 cap_us=3806 match_us=40301	44107
234308235	worker	imagefind_poll	#1 poll=73 matched=no conf=0.452 cap_us=4109 match_us=39563	43672
234659608	worker	imagefind_poll	#1 poll=74 matched=no conf=0.445 cap_us=3639 match_us=39849	43488
235010246	worker	imagefind_poll	#1 poll=75 matched=no conf=0.452 cap_us=3630 match_us=39938	43568
235360755	worker	imagefind_poll	#1 poll=76 matched=no conf=0.445 cap_us=3719 match_us=39873	43592
235711264	worker	imagefind_poll	#1 poll=77 matched=no conf=0.452 cap_us=3639 match_us=39701	43340
236062640	worker	imagefind_poll	#1 poll=78 matched=no conf=0.445 cap_us=4184 match_us=39871	44055
236413631	worker	imagefind_poll	#1 poll=79 matched=no conf=0.452 cap_us=4089 match_us=39602	43691
236764569	worker	imagefind_poll	#1 poll=80 matched=no conf=0.445 cap_us=4207 match_us=39886	44093
237116722	worker	imagefind_poll	#1 poll=81 matched=no conf=0.452 cap_us=3815 match_us=39625	43440
237466536	worker	imagefind_poll	#1 poll=82 matched=no conf=0.445 cap_us=3620 match_us=39148	42768
237817349	worker	imagefind_poll	#1 poll=83 matched=no conf=0.452 cap_us=3905 match_us=39605	43510
238167882	worker	imagefind_poll	#1 poll=84 matched=no conf=0.445 cap_us=3675 match_us=39408	43083
238519103	worker	imagefind_poll	#1 poll=85 matched=no conf=0.452 cap_us=4046 match_us=39570	43616
238869969	worker	imagefind_poll	#1 poll=86 matched=no conf=0.445 cap_us=4090 match_us=39598	43688
239220418	worker	imagefind_poll	#1 poll=87 matched=no conf=0.452 cap_us=3964 match_us=39395	43359
239531865	worker	capture_imagefind	area=2	4491
239571751	worker	imagefind_poll	#1 poll=88 matched=no conf=0.445 cap_us=4567 match_us=39675	44242
239922242	worker	imagefind_poll	#1 poll=89 matched=no conf=0.452 cap_us=3912 match_us=39443	43355
240273909	worker	imagefind_poll	#1 poll=90 matched=yes conf=1.000 cap_us=4891 match_us=39944	44835
240274169	worker	spike_loop_duration	iter=0	35573150
240279602	ui	trigger_action_start	feature=수락 block=#1
240323013	worker	synthetic_mouse_click	synthetic=1 path=screen x=1917 y=1299	39670
240323078	worker	spike_loop_duration	iter=0	43095
240325763	ui	trigger_cooldown_start	ms=10000
242347671	ui	trigger_monitor_start	block=#1
242347845	worker	spike_loop_gap	after_loop=0 gap_us=2024767	2024767
242362501	worker	imagefind_poll	#1 poll=1 matched=no conf=0.303 cap_us=3459 match_us=540	3999
250318985	ui	trigger_monitor_start	block=#1
250319167	worker	spike_loop_gap	after_loop=0 gap_us=9996090	9996090
250368254	worker	imagefind_poll	#1 poll=1 matched=yes conf=0.961 cap_us=3647 match_us=39802	43449
250368444	worker	spike_loop_duration	iter=0	49275
250374343	ui	trigger_action_start	feature=수락 block=#1
250413522	worker	synthetic_mouse_click	synthetic=1 path=screen x=1917 y=1299	35427
250413551	worker	spike_loop_duration	iter=0	38883
250415578	ui	trigger_cooldown_start	ms=10000
256762877	worker	capture_imagefind	area=2	3717
260410353	ui	trigger_monitor_start	block=#1
260410537	worker	spike_loop_gap	after_loop=0 gap_us=9996984	9996984
260458668	worker	imagefind_poll	#1 poll=1 matched=yes conf=1.000 cap_us=3606 match_us=39590	43196
260458865	worker	spike_loop_duration	iter=0	48327
260464818	ui	trigger_action_start	feature=수락 block=#1
260506516	worker	synthetic_mouse_click	synthetic=1 path=screen x=1917 y=1299	38135
260506543	worker	spike_loop_duration	iter=0	41413
260508699	ui	trigger_cooldown_start	ms=10000
270504287	ui	trigger_monitor_start	block=#1
270504486	worker	spike_loop_gap	after_loop=0 gap_us=9997941	9997941
270554538	worker	imagefind_poll	#1 poll=1 matched=no conf=0.485 cap_us=4460 match_us=39442	43902
270906645	worker	imagefind_poll	#1 poll=2 matched=no conf=0.485 cap_us=4160 match_us=39856	44016
271218202	worker	capture_imagefind	area=2	3981
271257779	worker	imagefind_poll	#1 poll=3 matched=no conf=0.485 cap_us=4056 match_us=39383	43439
271608508	worker	imagefind_poll	#1 poll=4 matched=no conf=0.485 cap_us=3703 match_us=39537	43240
271960371	worker	imagefind_poll	#1 poll=5 matched=no conf=0.485 cap_us=3999 match_us=40066	44065
272310857	worker	imagefind_poll	#1 poll=6 matched=no conf=0.485 cap_us=3862 match_us=39130	42992
272661583	worker	imagefind_poll	#1 poll=7 matched=no conf=0.485 cap_us=4053 match_us=39613	43666
273012857	worker	imagefind_poll	#1 poll=8 matched=no conf=0.485 cap_us=3965 match_us=39918	43883
273365273	worker	imagefind_poll	#1 poll=9 matched=no conf=0.485 cap_us=4438 match_us=40467	44905
273716131	worker	imagefind_poll	#1 poll=10 matched=no conf=0.485 cap_us=3813 match_us=39642	43455
274067587	worker	imagefind_poll	#1 poll=11 matched=no conf=0.485 cap_us=3845 match_us=39697	43542
274418402	worker	imagefind_poll	#1 poll=12 matched=no conf=0.485 cap_us=3809 match_us=39686	43495
274768302	worker	imagefind_poll	#1 poll=13 matched=no conf=0.485 cap_us=3690 match_us=39101	42791
275118822	worker	imagefind_poll	#1 poll=14 matched=no conf=0.485 cap_us=3770 match_us=39291	43061
275469294	worker	imagefind_poll	#1 poll=15 matched=no conf=0.485 cap_us=3952 match_us=39606	43558
275819809	worker	imagefind_poll	#1 poll=16 matched=no conf=0.485 cap_us=3822 match_us=39553	43375
276170728	worker	imagefind_poll	#1 poll=17 matched=no conf=0.485 cap_us=3932 match_us=39605	43537
276521116	worker	imagefind_poll	#1 poll=18 matched=no conf=0.485 cap_us=3962 match_us=39581	43543
276872465	worker	imagefind_poll	#1 poll=19 matched=no conf=0.485 cap_us=4021 match_us=39583	43604
277223562	worker	imagefind_poll	#1 poll=20 matched=no conf=0.485 cap_us=4162 match_us=39658	43820
277573969	worker	imagefind_poll	#1 poll=21 matched=no conf=0.485 cap_us=3819 match_us=39250	43069
277925315	worker	imagefind_poll	#1 poll=22 matched=no conf=0.485 cap_us=3880 match_us=39325	43205
278275232	worker	imagefind_poll	#1 poll=23 matched=no conf=0.485 cap_us=3758 match_us=39263	43021
278625298	worker	imagefind_poll	#1 poll=24 matched=no conf=0.485 cap_us=3684 match_us=39041	42725
278975679	worker	imagefind_poll	#1 poll=25 matched=no conf=0.485 cap_us=3575 match_us=39824	43399
279326764	worker	imagefind_poll	#1 poll=26 matched=no conf=0.485 cap_us=3988 match_us=39668	43656
279576018	worker	capture_imagefind	area=2	3476
279677802	worker	imagefind_poll	#1 poll=27 matched=no conf=0.485 cap_us=4057 match_us=39350	43407
280027654	worker	imagefind_poll	#1 poll=28 matched=no conf=0.485 cap_us=3888 match_us=39172	43060
280380208	worker	imagefind_poll	#1 poll=29 matched=no conf=0.485 cap_us=4449 match_us=39453	43902
280731308	worker	imagefind_poll	#1 poll=30 matched=no conf=0.485 cap_us=4119 match_us=39787	43906
281082026	worker	imagefind_poll	#1 poll=31 matched=no conf=0.485 cap_us=4078 match_us=39436	43514
281433052	worker	imagefind_poll	#1 poll=32 matched=no conf=0.485 cap_us=3545 match_us=39446	42991
281783529	worker	imagefind_poll	#1 poll=33 matched=no conf=0.485 cap_us=3604 match_us=39828	43432
282134420	worker	imagefind_poll	#1 poll=34 matched=no conf=0.485 cap_us=3627 match_us=39583	43210
282484948	worker	imagefind_poll	#1 poll=35 matched=no conf=0.485 cap_us=3800 match_us=39579	43379
282835478	worker	imagefind_poll	#1 poll=36 matched=no conf=0.485 cap_us=3882 match_us=39533	43415
283185539	worker	imagefind_poll	#1 poll=37 matched=no conf=0.485 cap_us=3818 match_us=39244	43062
283536566	worker	imagefind_poll	#1 poll=38 matched=no conf=0.485 cap_us=3929 match_us=39214	43143
283887425	worker	imagefind_poll	#1 poll=39 matched=no conf=0.485 cap_us=4102 match_us=39412	43514
284239595	worker	imagefind_poll	#1 poll=40 matched=no conf=0.485 cap_us=3657 match_us=39641	43298
284590447	worker	imagefind_poll	#1 poll=41 matched=no conf=0.485 cap_us=4226 match_us=39211	43437
284940263	worker	imagefind_poll	#1 poll=42 matched=no conf=0.485 cap_us=3665 match_us=39198	42863
285290623	worker	imagefind_poll	#1 poll=43 matched=no conf=0.485 cap_us=3748 match_us=39207	42955
285641740	worker	imagefind_poll	#1 poll=44 matched=no conf=0.485 cap_us=3671 match_us=39384	43055
285992205	worker	imagefind_poll	#1 poll=45 matched=no conf=0.485 cap_us=3778 match_us=39427	43205
286342819	worker	imagefind_poll	#1 poll=46 matched=no conf=0.485 cap_us=3627 match_us=39804	43431
286693641	worker	imagefind_poll	#1 poll=47 matched=no conf=0.485 cap_us=3881 match_us=39417	43298
287043788	worker	imagefind_poll	#1 poll=48 matched=no conf=0.485 cap_us=3869 match_us=39070	42939
287394906	worker	imagefind_poll	#1 poll=49 matched=no conf=0.485 cap_us=4116 match_us=39755	43871
287705804	worker	capture_imagefind	area=2	4126
287745272	worker	imagefind_poll	#1 poll=50 matched=no conf=0.485 cap_us=4201 match_us=39267	43468
288097104	worker	imagefind_poll	#1 poll=51 matched=no conf=0.485 cap_us=4408 match_us=40014	44422
288448076	worker	imagefind_poll	#1 poll=52 matched=no conf=0.485 cap_us=3790 match_us=39737	43527
288801330	worker	imagefind_poll	#1 poll=53 matched=no conf=0.485 cap_us=3730 match_us=39328	43058
289151432	worker	imagefind_poll	#1 poll=54 matched=no conf=0.485 cap_us=3714 match_us=39233	42947
289502506	worker	imagefind_poll	#1 poll=55 matched=no conf=0.485 cap_us=3959 match_us=39447	43406
289853389	worker	imagefind_poll	#1 poll=56 matched=no conf=0.485 cap_us=4320 match_us=39435	43755
290204725	worker	imagefind_poll	#1 poll=57 matched=no conf=0.485 cap_us=4351 match_us=39523	43874
290556543	worker	imagefind_poll	#1 poll=58 matched=no conf=0.485 cap_us=4434 match_us=40286	44720
290906868	worker	imagefind_poll	#1 poll=59 matched=no conf=0.485 cap_us=3769 match_us=39210	42979
291258805	worker	imagefind_poll	#1 poll=60 matched=no conf=0.485 cap_us=4455 match_us=40239	44694
291609829	worker	imagefind_poll	#1 poll=61 matched=no conf=0.485 cap_us=3936 match_us=39397	43333
291960704	worker	imagefind_poll	#1 poll=62 matched=no conf=0.485 cap_us=3778 match_us=39587	43365
292312756	worker	imagefind_poll	#1 poll=63 matched=no conf=0.485 cap_us=4893 match_us=39590	44483
292663479	worker	imagefind_poll	#1 poll=64 matched=no conf=0.485 cap_us=4215 match_us=39642	43857
293015208	worker	imagefind_poll	#1 poll=65 matched=no conf=0.485 cap_us=4087 match_us=39758	43845
293366200	worker	imagefind_poll	#1 poll=66 matched=no conf=0.485 cap_us=3573 match_us=40427	44000
293715938	worker	imagefind_poll	#1 poll=67 matched=no conf=0.485 cap_us=3637 match_us=39092	42729
294102464	worker	imagefind_poll	#1 poll=68 matched=no conf=0.485 cap_us=5214 match_us=39227	44441
294454099	worker	imagefind_poll	#1 poll=69 matched=no conf=0.485 cap_us=4275 match_us=39678	43953
294804728	worker	imagefind_poll	#1 poll=70 matched=no conf=0.485 cap_us=3969 match_us=39399	43368
295155768	worker	imagefind_poll	#1 poll=71 matched=no conf=0.485 cap_us=4407 match_us=39664	44071
295506995	worker	imagefind_poll	#1 poll=72 matched=no conf=0.485 cap_us=4081 match_us=39742	43823
295858868	worker	imagefind_poll	#1 poll=73 matched=no conf=0.485 cap_us=4125 match_us=40840	44965
296100124	worker	capture_imagefind	area=2	3806
296208632	worker	imagefind_poll	#1 poll=74 matched=no conf=0.485 cap_us=3631 match_us=39318	42949
296558418	worker	imagefind_poll	#1 poll=75 matched=no conf=0.485 cap_us=3631 match_us=39180	42811
296909083	worker	imagefind_poll	#1 poll=76 matched=no conf=0.485 cap_us=3834 match_us=39797	43631
297261124	worker	imagefind_poll	#1 poll=77 matched=no conf=0.485 cap_us=4503 match_us=39908	44411
297611965	worker	imagefind_poll	#1 poll=78 matched=no conf=0.485 cap_us=4142 match_us=39714	43856
297962650	worker	imagefind_poll	#1 poll=79 matched=no conf=0.485 cap_us=4240 match_us=40046	44286
298313172	worker	imagefind_poll	#1 poll=80 matched=no conf=0.485 cap_us=4005 match_us=39285	43290
298662748	worker	imagefind_poll	#1 poll=81 matched=no conf=0.485 cap_us=3884 match_us=39312	43196
299012668	worker	imagefind_poll	#1 poll=82 matched=no conf=0.485 cap_us=3851 match_us=39382	43233
299364642	worker	imagefind_poll	#1 poll=83 matched=no conf=0.485 cap_us=4414 match_us=40333	44747
299715104	worker	imagefind_poll	#1 poll=84 matched=no conf=0.485 cap_us=4405 match_us=39310	43715
300065519	worker	imagefind_poll	#1 poll=85 matched=no conf=0.485 cap_us=3808 match_us=39603	43411
300415885	worker	imagefind_poll	#1 poll=86 matched=no conf=0.485 cap_us=3624 match_us=39308	42932
300766077	worker	imagefind_poll	#1 poll=87 matched=no conf=0.485 cap_us=3808 match_us=39072	42880
301116062	worker	imagefind_poll	#1 poll=88 matched=no conf=0.485 cap_us=3651 match_us=39112	42763
301468376	worker	imagefind_poll	#1 poll=89 matched=no conf=0.485 cap_us=4224 match_us=40978	45202
301818994	worker	imagefind_poll	#1 poll=90 matched=no conf=0.485 cap_us=3848 match_us=39861	43709
302169299	worker	imagefind_poll	#1 poll=91 matched=no conf=0.485 cap_us=4149 match_us=39101	43250
302519989	worker	imagefind_poll	#1 poll=92 matched=no conf=0.485 cap_us=4203 match_us=39144	43347
303182760	ui	switchToProfile.maybeSave	elapsed_ms=0	0
303182917	ui	switchToProfile.saveSettings	elapsed_ms=0	0
303183384	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
303183416	ui	switchToProfile.stopSessions	elapsed_ms=0	0
303184791	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
303185277	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
303185488	ui	refreshWorkflowEditor	elapsed_ms=0	0
303185812	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
303185988	ui	refreshWorkflowEditor	elapsed_ms=0	0
303186177	ui	syncHotkeys	elapsed_ms=0	0
303186223	worker	spike_loop_duration	iter=0	32681734
303186263	ui	loadProjectFromFile	elapsed_ms=2	2000
303186293	ui	loadActiveProfile	elapsed_ms=2	2000
303186302	ui	switchToProfile.loadActiveProfile	elapsed_ms=2	2000
303188448	ui	switchToProfile	elapsed_ms=6	6000
303188474	ui	profile_switch	id=1f4a4367-387b-4984-9019-1b6fc9ad35d8 auto=yes	6033
303192297	worker	spike_loop_duration	iter=0	60844450
304453204	ui	switchToProfile.maybeSave	elapsed_ms=0	0
304453354	ui	switchToProfile.saveSettings	elapsed_ms=0	0
304453669	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
304453704	ui	switchToProfile.stopSessions	elapsed_ms=0	0
304455743	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
304468888	ui	WorkflowEditorPanel.refresh	elapsed_ms=8	8000
304470949	ui	refreshWorkflowEditor	elapsed_ms=11	11000
304484543	ui	WorkflowEditorPanel.refresh	elapsed_ms=8	8000
304487239	ui	refreshWorkflowEditor	elapsed_ms=15	15000
304487765	ui	syncHotkeys	elapsed_ms=0	0
304487909	ui	loadProjectFromFile	elapsed_ms=33	33000
304487944	ui	loadActiveProfile	elapsed_ms=33	33000
304487954	ui	switchToProfile.loadActiveProfile	elapsed_ms=33	33000
304508974	ui	switchToProfile	elapsed_ms=57	57000
304509056	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	57613
304545730	ui	WorkflowEditorPanel.refresh	elapsed_ms=6	6000
304546597	ui	refreshWorkflowEditor	elapsed_ms=8	8000
304571039	ui	session_end	reason=preempted_by_new_session
304571104	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
304574621	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
304575274	ui	refreshWorkflowEditor	elapsed_ms=2	2000
304576601	ui	trigger_monitor_start	block=#1
304576879	ui	session_end	reason=preempted_by_new_session
304576964	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
304583596	ui	WorkflowEditorPanel.refresh	elapsed_ms=4	4000
304586979	ui	refreshWorkflowEditor	elapsed_ms=9	9000
304592068	ui	trigger_monitor_start	block=#1
389419813	ui	session_end	reason=preempted_by_new_session
389419884	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
389422627	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3240
389426752	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
389427435	ui	refreshWorkflowEditor	elapsed_ms=4	4000
389465123	worker	spike_loop_duration	iter=0	40180
389465678	ui	ui_fast_repeat_flush	pending_iters=1	394
389512247	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1077
389517555	ui	session_end	finish success=yes
389604353	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
389606594	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2715
389608239	worker	spike_loop_gap	after_loop=0 gap_us=143116	143116
389608548	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
389608942	ui	refreshWorkflowEditor	elapsed_ms=1	1000
389646371	worker	spike_loop_duration	iter=0	38127
389646954	ui	ui_fast_repeat_flush	pending_iters=1	393
389682467	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1089
389692077	ui	session_end	finish success=yes
389823436	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
389826679	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3713
389828264	worker	spike_loop_gap	after_loop=0 gap_us=181894	181894
389828811	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
389829230	ui	refreshWorkflowEditor	elapsed_ms=2	2000
389866333	worker	spike_loop_duration	iter=0	38063
389867091	ui	ui_fast_repeat_flush	pending_iters=1	513
389923703	worker	loop_interval_sleep	requested_ms=56	57220
389923712	worker	spike_loop_gap	after_loop=0 gap_us=57381	57381
389928367	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1956
389960648	worker	spike_loop_duration	iter=1 loop=1	36932
389961067	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	357
389961102	ui	session_end	finish success=yes loop=1
392946925	ui	hotkey_trigger_dispatch	aad48461-acdc-4484-a316-0637c4b4b8f3	12
392947401	ui	session_begin	feature=어그로 mode=RepeatCount profile=LOL source=hotkey blocks=3
392949234	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
392949703	ui	refreshWorkflowEditor	elapsed_ms=2	2000
392954889	worker	spike_loop_gap	after_loop=1 gap_us=2994240	2994240
393078952	worker	spike_loop_duration	iter=0	124060
393079842	ui	ui_fast_repeat_flush	pending_iters=1	464
393080025	ui	session_end	finish success=yes
437289577	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
437292082	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3012
437293651	worker	spike_loop_gap	after_loop=0 gap_us=44214697	44214697
437294901	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
437295480	ui	refreshWorkflowEditor	elapsed_ms=2	2000
437336225	worker	spike_loop_duration	iter=0	42565
437346021	ui	ui_fast_repeat_flush	pending_iters=1	636
437372094	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1233
437377404	ui	session_end	finish success=yes
437467368	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
437470373	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3508
437472742	worker	spike_loop_gap	after_loop=0 gap_us=136520	136520
437513911	worker	spike_loop_duration	iter=0	41164
437514911	ui	ui_fast_repeat_flush	pending_iters=1	728
437554271	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1399
437556591	ui	session_end	finish success=yes
444382315	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
444385019	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3179
444386563	worker	spike_loop_gap	after_loop=0 gap_us=6872653	6872653
444426474	worker	spike_loop_duration	iter=0	39906
444427140	ui	ui_fast_repeat_flush	pending_iters=1	422
444471606	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1261
444478508	ui	session_end	finish success=yes
444559823	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
444562445	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3088
444564033	worker	spike_loop_gap	after_loop=0 gap_us=137560	137560
444603214	worker	spike_loop_duration	iter=0	39178
444603891	ui	ui_fast_repeat_flush	pending_iters=1	375
444642175	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1180
444646697	ui	session_end	finish success=yes
444700572	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
444703666	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4150
444706088	worker	spike_loop_gap	after_loop=0 gap_us=102873	102873
444746887	worker	spike_loop_duration	iter=0	40794
444747478	ui	ui_fast_repeat_flush	pending_iters=1	419
444825298	worker	loop_interval_sleep	requested_ms=77	78305
444825311	worker	spike_loop_gap	after_loop=0 gap_us=78423	78423
444840368	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1446
444862210	worker	spike_loop_duration	iter=1 loop=1	36896
444863214	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	596
444863219	ui	session_end	finish success=yes loop=1
446313233	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
446315769	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3007
446317633	worker	spike_loop_gap	after_loop=1 gap_us=1455422	1455422
446357852	worker	spike_loop_duration	iter=0	40215
446358495	ui	ui_fast_repeat_flush	pending_iters=1	453
446431350	worker	loop_interval_sleep	requested_ms=72	73389
446431360	worker	spike_loop_gap	after_loop=0 gap_us=73507	73507
446438301	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1205
446468189	worker	spike_loop_duration	iter=1 loop=1	36826
446468723	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	449
446468758	ui	session_end	finish success=yes loop=1
446765120	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
446768037	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3392
446769696	worker	spike_loop_gap	after_loop=1 gap_us=301506	301506
446810470	worker	spike_loop_duration	iter=0	40770
446811334	ui	ui_fast_repeat_flush	pending_iters=1	610
446862751	worker	loop_interval_sleep	requested_ms=51	52166
446862757	worker	spike_loop_gap	after_loop=0 gap_us=52287	52287
446866513	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1156
446899019	worker	spike_loop_duration	iter=1 loop=1	36260
446899564	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	377
446899635	ui	session_end	finish success=yes loop=1
447459783	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
447462502	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3371
447464211	worker	spike_loop_gap	after_loop=1 gap_us=565192	565192
447503883	worker	spike_loop_duration	iter=0	39641
447504511	ui	ui_fast_repeat_flush	pending_iters=1	413
447546341	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1058
447555281	ui	session_end	finish success=yes
447766789	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
447769613	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3452
447771477	worker	spike_loop_gap	after_loop=0 gap_us=267622	267622
447771981	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
447772382	ui	refreshWorkflowEditor	elapsed_ms=2	2000
447808949	worker	spike_loop_duration	iter=0	37468
447809545	ui	ui_fast_repeat_flush	pending_iters=1	437
447859384	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1053
447863907	ui	session_end	finish success=yes
449246746	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
449249194	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2890
449250828	worker	spike_loop_gap	after_loop=0 gap_us=1441880	1441880
449251246	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
449251664	ui	refreshWorkflowEditor	elapsed_ms=2	2000
449288790	worker	spike_loop_duration	iter=0	37958
449289328	ui	ui_fast_repeat_flush	pending_iters=1	388
449324920	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	967
449330881	ui	session_end	finish success=yes
450153743	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
450156346	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3083
450158054	worker	spike_loop_gap	after_loop=0 gap_us=869264	869264
450197566	worker	spike_loop_duration	iter=0	39507
450198365	ui	ui_fast_repeat_flush	pending_iters=1	588
450253944	worker	loop_interval_sleep	requested_ms=55	56265
450253952	worker	spike_loop_gap	after_loop=0 gap_us=56387	56387
450259773	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1133
450290745	worker	spike_loop_duration	iter=1 loop=1	36790
450291212	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	367
450291246	ui	session_end	finish success=yes loop=1
451529722	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
451532708	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3445
451534480	worker	spike_loop_gap	after_loop=1 gap_us=1243734	1243734
451574819	worker	spike_loop_duration	iter=0	40332
451575768	ui	ui_fast_repeat_flush	pending_iters=1	689
451628814	worker	loop_interval_sleep	requested_ms=53	53892
451628822	worker	spike_loop_gap	after_loop=0 gap_us=54006	54006
451657039	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1276
451665102	worker	spike_loop_duration	iter=1 loop=1	36277
451665497	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	337
451665527	ui	session_end	finish success=yes loop=1
452063004	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
452066008	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3502
452067782	worker	spike_loop_gap	after_loop=1 gap_us=402679	402679
452107495	worker	spike_loop_duration	iter=0	39708
452108161	ui	ui_fast_repeat_flush	pending_iters=1	439
452177015	worker	loop_interval_sleep	requested_ms=68	69444
452177021	worker	spike_loop_gap	after_loop=0 gap_us=69528	69528
452187516	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1100
452215849	worker	spike_loop_duration	iter=1 loop=1	38824
452216716	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	670
452216758	ui	session_end	finish success=yes loop=1
453729553	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
453731999	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2983
453734035	worker	spike_loop_gap	after_loop=1 gap_us=1518185	1518185
453776311	worker	spike_loop_duration	iter=0	42272
453777205	ui	ui_fast_repeat_flush	pending_iters=1	667
453850790	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1303
453855031	ui	session_end	finish success=yes
455857215	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
455860044	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3297
455861829	worker	spike_loop_gap	after_loop=0 gap_us=2085519	2085519
455898808	worker	spike_loop_duration	iter=0	36975
455899512	ui	ui_fast_repeat_flush	pending_iters=1	521
455963413	worker	loop_interval_sleep	requested_ms=63	64509
455963420	worker	spike_loop_gap	after_loop=0 gap_us=64613	64613
455988721	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1630
456000764	worker	spike_loop_duration	iter=1 loop=1	37342
456002209	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	961
456002216	ui	session_end	finish success=yes loop=1
458305459	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
458307791	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3072
458309476	worker	spike_loop_gap	after_loop=1 gap_us=2308711	2308711
458347135	worker	spike_loop_duration	iter=0	37656
458347971	ui	ui_fast_repeat_flush	pending_iters=1	482
458411508	worker	loop_interval_sleep	requested_ms=63	64249
458411514	worker	spike_loop_gap	after_loop=0 gap_us=64379	64379
458448779	worker	spike_loop_duration	iter=1 loop=1	37263
458449205	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	364
458500014	worker	loop_interval_sleep	requested_ms=50 loop=1	51110
458500020	worker	spike_loop_gap	after_loop=1 gap_us=51240 loop=1	51240
458536697	worker	spike_loop_duration	iter=2 loop=2	36675
458537246	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	468
458612420	worker	loop_interval_sleep	requested_ms=74 loop=2	75471
458612426	worker	spike_loop_gap	after_loop=2 gap_us=75728 loop=2	75728
458649108	worker	spike_loop_duration	iter=3 loop=3	36680
458649566	ui	ui_fast_repeat_flush	pending_iters=1 loop=3	326
458675118	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=3	1241
458683004	ui	session_end	finish success=yes loop=3
463588259	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
463590098	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2251
463592288	worker	spike_loop_gap	after_loop=3 gap_us=4943179	4943179
463632899	worker	spike_loop_duration	iter=0	40609
463637202	ui	ui_fast_repeat_flush	pending_iters=1	397
463680464	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1172
463686019	ui	session_end	finish success=yes
463792829	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
463795584	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3281
463797209	worker	spike_loop_gap	after_loop=0 gap_us=164308	164308
463837122	worker	spike_loop_duration	iter=0	39911
463837765	ui	ui_fast_repeat_flush	pending_iters=1	390
463887022	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	946
463889715	ui	session_end	finish success=yes
488447119	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
488450261	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3698
488452706	worker	spike_loop_gap	after_loop=0 gap_us=24615578	24615578
488489974	worker	spike_loop_duration	iter=0	37264
488492185	ui	ui_fast_repeat_flush	pending_iters=1	448
488537124	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1104
488541471	ui	session_end	finish success=yes
488641978	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
488644691	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3579
488647337	worker	spike_loop_gap	after_loop=0 gap_us=157361	157361
488687353	worker	spike_loop_duration	iter=0	40012
488688312	ui	ui_fast_repeat_flush	pending_iters=1	440
488731799	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1093
488754946	ui	session_end	finish success=yes
488792427	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
488794702	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2729
488796547	worker	spike_loop_gap	after_loop=0 gap_us=109194	109194
488841582	worker	spike_loop_duration	iter=0	45033
488842714	ui	ui_fast_repeat_flush	pending_iters=1	408
488886372	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	964
488892734	ui	session_end	finish success=yes
493046703	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
493049793	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3661
493051642	worker	spike_loop_gap	after_loop=0 gap_us=4210055	4210055
493089050	worker	spike_loop_duration	iter=0	37405
493089621	ui	ui_fast_repeat_flush	pending_iters=1	418
493131364	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1513
493140899	ui	session_end	finish success=yes
493222339	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
493225760	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4016
493227410	worker	spike_loop_gap	after_loop=0 gap_us=138359	138359
493267254	worker	spike_loop_duration	iter=0	39840
493267851	ui	ui_fast_repeat_flush	pending_iters=1	407
493325373	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	8124
493329018	ui	session_end	finish success=yes
493387766	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
493389583	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2139
493391262	worker	spike_loop_gap	after_loop=0 gap_us=124008	124008
493430999	worker	spike_loop_duration	iter=0	39734
493431722	ui	ui_fast_repeat_flush	pending_iters=1	385
493472670	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1499
493474067	ui	session_end	finish success=yes
493543461	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
493546452	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4125
493548665	worker	spike_loop_gap	after_loop=0 gap_us=117663	117663
493586202	worker	spike_loop_duration	iter=0	37534
493586990	ui	ui_fast_repeat_flush	pending_iters=1	417
493658942	worker	loop_interval_sleep	requested_ms=71	72686
493658950	worker	spike_loop_gap	after_loop=0 gap_us=72747	72747
493665783	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1365
493695313	worker	spike_loop_duration	iter=1 loop=1	36360
493695950	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	558
493696139	ui	session_end	finish success=yes loop=1
505681902	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
505684278	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2892
505686046	worker	spike_loop_gap	after_loop=1 gap_us=11990731	11990731
505725977	worker	spike_loop_duration	iter=0	39927
505726948	ui	ui_fast_repeat_flush	pending_iters=1	411
505791256	worker	loop_interval_sleep	requested_ms=64	65182
505791262	worker	spike_loop_gap	after_loop=0 gap_us=65286	65286
505827769	worker	spike_loop_duration	iter=1 loop=1	36504
505828400	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	564
505831754	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1166
505838891	ui	session_end	finish success=yes loop=1
511115462	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
511118135	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3502
511119925	worker	spike_loop_gap	after_loop=1 gap_us=5292155	5292155
511157579	worker	spike_loop_duration	iter=0	37650
511158140	ui	ui_fast_repeat_flush	pending_iters=1	398
511216231	worker	loop_interval_sleep	requested_ms=57	58559
511216237	worker	spike_loop_gap	after_loop=0 gap_us=58659	58659
511252910	worker	spike_loop_duration	iter=1 loop=1	36671
511253365	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	391
511301716	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1444
511325689	ui	session_end	finish success=yes loop=1
516177610	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
516180038	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2918
516182180	worker	spike_loop_gap	after_loop=1 gap_us=4929270	4929270
516227411	worker	spike_loop_duration	iter=0	45226
516228217	ui	ui_fast_repeat_flush	pending_iters=1	532
516283731	worker	loop_interval_sleep	requested_ms=55	56255
516283736	worker	spike_loop_gap	after_loop=0 gap_us=56327	56327
516303021	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1121
516322423	worker	spike_loop_duration	iter=1 loop=1	38684
516322874	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	391
516322904	ui	session_end	finish success=yes loop=1
518867522	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
518870244	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3194
518871840	worker	spike_loop_gap	after_loop=1 gap_us=2549417	2549417
518872313	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
518872771	ui	refreshWorkflowEditor	elapsed_ms=2	2000
518910292	worker	spike_loop_duration	iter=0	38447
518910874	ui	ui_fast_repeat_flush	pending_iters=1	421
518989743	worker	loop_interval_sleep	requested_ms=78	79353
518989749	worker	spike_loop_gap	after_loop=0 gap_us=79459	79459
519026509	worker	spike_loop_duration	iter=1 loop=1	36757
519026892	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	337
519059051	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1419
519067452	ui	session_end	finish success=yes loop=1
520928401	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
520931048	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3169
520932820	worker	spike_loop_gap	after_loop=1 gap_us=1906310	1906310
520933102	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
520933498	ui	refreshWorkflowEditor	elapsed_ms=1	1000
520970357	worker	spike_loop_duration	iter=0	37533
520971318	ui	ui_fast_repeat_flush	pending_iters=1	715
521015654	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1328
521023857	ui	session_end	finish success=yes
521107033	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
521109785	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3233
521111549	worker	spike_loop_gap	after_loop=0 gap_us=141192	141192
521152571	worker	spike_loop_duration	iter=0	41017
521154219	ui	ui_fast_repeat_flush	pending_iters=1	689
521194999	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1486
521203985	ui	session_end	finish success=yes
521249938	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
521252395	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2921
521254851	worker	spike_loop_gap	after_loop=0 gap_us=102283	102283
521295779	worker	spike_loop_duration	iter=0	40924
521296975	ui	ui_fast_repeat_flush	pending_iters=1	522
521358720	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1283
521364058	ui	session_end	finish success=yes
534218192	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
534221323	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3599
534223321	worker	spike_loop_gap	after_loop=0 gap_us=12927542	12927542
534260926	worker	spike_loop_duration	iter=0	37601
534261614	ui	ui_fast_repeat_flush	pending_iters=1	517
534308140	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1714
534312293	ui	session_end	finish success=yes
539719635	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
539722797	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3661
539725160	worker	spike_loop_gap	after_loop=0 gap_us=5464233	5464233
539762836	worker	spike_loop_duration	iter=0	37672
539763403	ui	ui_fast_repeat_flush	pending_iters=1	353
539813604	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1158
539819019	ui	session_end	finish success=yes
555803007	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
555805625	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3262
555807940	worker	spike_loop_gap	after_loop=0 gap_us=16045104	16045104
555844692	worker	spike_loop_duration	iter=0	36748
555845258	ui	ui_fast_repeat_flush	pending_iters=1	397
555908748	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1230
555916321	ui	session_end	finish success=yes
560471993	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
560474859	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3376
560476608	worker	spike_loop_gap	after_loop=0 gap_us=4631917	4631917
560513948	worker	spike_loop_duration	iter=0	37338
560515110	ui	ui_fast_repeat_flush	pending_iters=1	752
560585733	worker	loop_interval_sleep	requested_ms=70	71688
560585738	worker	spike_loop_gap	after_loop=0 gap_us=71789	71789
560610551	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1129
560624758	worker	spike_loop_duration	iter=1 loop=1	39016
560625253	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	436
560625294	ui	session_end	finish success=yes loop=1
562918203	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
562920755	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3131
562922353	worker	spike_loop_gap	after_loop=1 gap_us=2297593	2297593
562962783	worker	spike_loop_duration	iter=0	40426
562963416	ui	ui_fast_repeat_flush	pending_iters=1	439
563031246	worker	loop_interval_sleep	requested_ms=67	68365
563031252	worker	spike_loop_gap	after_loop=0 gap_us=68470	68470
563067563	worker	spike_loop_duration	iter=1 loop=1	36309
563068197	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	525
563139286	worker	loop_interval_sleep	requested_ms=70 loop=1	71541
563139293	worker	spike_loop_gap	after_loop=1 gap_us=71729 loop=1	71729
563190481	worker	spike_loop_duration	iter=2 loop=2	51185
563191138	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	476
563244866	worker	loop_interval_sleep	requested_ms=53 loop=2	54159
563244873	worker	spike_loop_gap	after_loop=2 gap_us=54392 loop=2	54392
563281100	worker	spike_loop_duration	iter=3 loop=3	36225
563281660	ui	ui_fast_repeat_flush	pending_iters=1 loop=3	470
563361383	worker	loop_interval_sleep	requested_ms=78 loop=3	80134
563361389	worker	spike_loop_gap	after_loop=3 gap_us=80290 loop=3	80290
563397596	worker	spike_loop_duration	iter=4 loop=4	36205
563397968	ui	ui_fast_repeat_flush	pending_iters=1 loop=4	308
563419931	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=4	1410
563428331	ui	session_end	finish success=yes loop=4
568524569	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
568527706	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3763
568529327	worker	spike_loop_gap	after_loop=4 gap_us=5131730	5131730
568566541	worker	spike_loop_duration	iter=0	37211
568567200	ui	ui_fast_repeat_flush	pending_iters=1	415
568611076	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1161
568617863	ui	session_end	finish success=yes
568722239	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
568724691	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3023
568726484	worker	spike_loop_gap	after_loop=0 gap_us=159941	159941
568766775	worker	spike_loop_duration	iter=0	40286
568767691	ui	ui_fast_repeat_flush	pending_iters=1	623
568811124	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1056
568818147	ui	session_end	finish success=yes
568890069	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
568892221	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2617
568893820	worker	spike_loop_gap	after_loop=0 gap_us=127044	127044
568935901	worker	spike_loop_duration	iter=0	42079
568936882	ui	ui_fast_repeat_flush	pending_iters=1	497
568988392	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1174
568992181	ui	session_end	finish success=yes
572839431	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
572841746	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2775
572843611	worker	spike_loop_gap	after_loop=0 gap_us=3907707	3907707
572881790	worker	spike_loop_duration	iter=0	38176
572885675	ui	ui_fast_repeat_flush	pending_iters=1	414
572920856	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2102
572924952	ui	session_end	finish success=yes
573014757	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
573018912	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4915
573021055	worker	spike_loop_gap	after_loop=0 gap_us=139261	139261
573058676	worker	spike_loop_duration	iter=0	37616
573059288	ui	ui_fast_repeat_flush	pending_iters=1	429
573110871	worker	loop_interval_sleep	requested_ms=51	52103
573110876	worker	spike_loop_gap	after_loop=0 gap_us=52201	52201
573112597	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2258
573153215	worker	spike_loop_duration	iter=1 loop=1	42337
573153646	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	356
573153787	ui	session_end	finish success=yes loop=1
576167272	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
576169772	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2999
576171558	worker	spike_loop_gap	after_loop=1 gap_us=3018343	3018343
576172108	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
576172728	ui	refreshWorkflowEditor	elapsed_ms=2	2000
576209681	worker	spike_loop_duration	iter=0	38118
576210359	ui	ui_fast_repeat_flush	pending_iters=1	409
576274159	worker	loop_interval_sleep	requested_ms=63	64405
576274166	worker	spike_loop_gap	after_loop=0 gap_us=64487	64487
576281563	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1063
576310330	worker	spike_loop_duration	iter=1 loop=1	36154
576310715	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	339
576310742	ui	session_end	finish success=yes loop=1
579052404	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
579054679	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2812
579056426	worker	spike_loop_gap	after_loop=1 gap_us=2746103	2746103
579056785	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
579057157	ui	refreshWorkflowEditor	elapsed_ms=2	2000
579093939	worker	spike_loop_duration	iter=0	37509
579094523	ui	ui_fast_repeat_flush	pending_iters=1	408
579159342	worker	loop_interval_sleep	requested_ms=64	65308
579159348	worker	spike_loop_gap	after_loop=0 gap_us=65410	65410
579195722	worker	spike_loop_duration	iter=1 loop=1	36372
579196228	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	304
579254131	worker	loop_interval_sleep	requested_ms=57 loop=1	58285
579254138	worker	spike_loop_gap	after_loop=1 gap_us=58415 loop=1	58415
579314793	worker	spike_loop_duration	iter=2 loop=2	60653
579315247	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	372
579393661	worker	loop_interval_sleep	requested_ms=77 loop=2	78714
579393665	worker	spike_loop_gap	after_loop=2 gap_us=78872 loop=2	78872
579394561	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=3	1087
579430854	worker	spike_loop_duration	iter=3 loop=3	37186
579431220	ui	ui_fast_repeat_flush	pending_iters=1 loop=3	311
579431434	ui	session_end	finish success=yes loop=3
580144512	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
580147027	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2989
580149021	worker	spike_loop_gap	after_loop=3 gap_us=718165	718165
580188760	worker	spike_loop_duration	iter=0	39733
580189424	ui	ui_fast_repeat_flush	pending_iters=1	462
580240316	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1125
580247040	ui	session_end	finish success=yes
580327462	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
580330642	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3656
580333820	worker	spike_loop_gap	after_loop=0 gap_us=145060	145060
580370562	worker	spike_loop_duration	iter=0	36738
580371624	ui	ui_fast_repeat_flush	pending_iters=1	760
580437590	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1051
580443080	ui	session_end	finish success=yes
580507680	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
580511120	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4012
580512605	worker	spike_loop_gap	after_loop=0 gap_us=142044	142044
580552542	worker	spike_loop_duration	iter=0	39932
580553368	ui	ui_fast_repeat_flush	pending_iters=1	502
580612623	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1352
580615668	ui	session_end	finish success=yes
580679780	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
580682326	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3037
580683989	worker	spike_loop_gap	after_loop=0 gap_us=131449	131449
580724311	worker	spike_loop_duration	iter=0	40318
580725071	ui	ui_fast_repeat_flush	pending_iters=1	432
580777586	worker	loop_interval_sleep	requested_ms=52	53116
580777593	worker	spike_loop_gap	after_loop=0 gap_us=53283	53283
580798683	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1297
580813996	worker	spike_loop_duration	iter=1 loop=1	36400
580814469	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	407
580814504	ui	session_end	finish success=yes loop=1
580865853	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
580869085	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3821
580870714	worker	spike_loop_gap	after_loop=1 gap_us=56716	56716
580932100	worker	spike_loop_duration	iter=0	61384
580936394	ui	ui_fast_repeat_flush	pending_iters=1	396
580992001	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1620
580998895	ui	session_end	finish success=yes
581155056	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
581157695	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3149
581159620	worker	spike_loop_gap	after_loop=0 gap_us=227517	227517
581199971	worker	spike_loop_duration	iter=0	40349
581200918	ui	ui_fast_repeat_flush	pending_iters=1	681
581259360	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1373
581261818	ui	session_end	finish success=yes
581423146	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
581425784	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3153
581427333	worker	spike_loop_gap	after_loop=0 gap_us=227361	227361
581467275	worker	spike_loop_duration	iter=0	39937
581467915	ui	ui_fast_repeat_flush	pending_iters=1	445
581519193	worker	loop_interval_sleep	requested_ms=51	51820
581519202	worker	spike_loop_gap	after_loop=0 gap_us=51928	51928
581541959	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1197
581556044	worker	spike_loop_duration	iter=1 loop=1	36840
581556448	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	338
581556482	ui	session_end	finish success=yes loop=1
581643684	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
581645327	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1936
581647134	worker	spike_loop_gap	after_loop=1 gap_us=91088	91088
581685999	worker	spike_loop_duration	iter=0	38862
581686707	ui	ui_fast_repeat_flush	pending_iters=1	384
581743183	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1418
581747415	ui	session_end	finish success=yes
581820990	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
581823631	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3106
581825178	worker	spike_loop_gap	after_loop=0 gap_us=139177	139177
581864379	worker	spike_loop_duration	iter=0	39199
581865170	ui	ui_fast_repeat_flush	pending_iters=1	424
581919482	worker	loop_interval_sleep	requested_ms=54	54973
581919491	worker	spike_loop_gap	after_loop=0 gap_us=55112	55112
581931673	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1275
581955795	worker	spike_loop_duration	iter=1 loop=1	36302
581956216	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	349
581956250	ui	session_end	finish success=yes loop=1
583218876	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
583222653	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	4292
583224202	worker	spike_loop_gap	after_loop=1 gap_us=1268406	1268406
583224830	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
583225219	ui	refreshWorkflowEditor	elapsed_ms=1	1000
583265806	worker	spike_loop_duration	iter=0	41602
583266404	ui	ui_fast_repeat_flush	pending_iters=1	412
583338750	worker	loop_interval_sleep	requested_ms=71	72827
583338757	worker	spike_loop_gap	after_loop=0 gap_us=72950	72950
583345192	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1622
583375235	worker	spike_loop_duration	iter=1 loop=1	36475
583375731	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	429
583375842	ui	session_end	finish success=yes loop=1
584078610	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
584081577	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3477
584083101	worker	spike_loop_gap	after_loop=1 gap_us=707865	707865
584083640	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
584084142	ui	refreshWorkflowEditor	elapsed_ms=1	1000
584128296	worker	spike_loop_duration	iter=0	45190
584129083	ui	ui_fast_repeat_flush	pending_iters=1	612
584192875	worker	loop_interval_sleep	requested_ms=63	64471
584192881	worker	spike_loop_gap	after_loop=0 gap_us=64587	64587
584222389	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1262
584229955	worker	spike_loop_duration	iter=1 loop=1	37070
584230475	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	334
584230479	ui	session_end	finish success=yes loop=1
585608889	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
585611666	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3266
585612916	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
585613963	worker	spike_loop_gap	after_loop=1 gap_us=1384008	1384008
585614285	ui	refreshWorkflowEditor	elapsed_ms=2	2000
585651225	worker	spike_loop_duration	iter=0	37258
585652234	ui	ui_fast_repeat_flush	pending_iters=1	731
585712635	worker	loop_interval_sleep	requested_ms=60	61317
585712642	worker	spike_loop_gap	after_loop=0 gap_us=61417	61417
585748977	worker	spike_loop_duration	iter=1 loop=1	36331
585749588	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	542
585804369	worker	loop_interval_sleep	requested_ms=54 loop=1	55268
585804376	worker	spike_loop_gap	after_loop=1 gap_us=55399 loop=1	55399
585835778	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=2	983
585870278	worker	spike_loop_duration	iter=2 loop=2	65900
585870684	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	320
585870715	ui	session_end	finish success=yes loop=2
590506714	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
590509127	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2914
590510685	worker	spike_loop_gap	after_loop=2 gap_us=4640404	4640404
590511250	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
590511799	ui	refreshWorkflowEditor	elapsed_ms=2	2000
590549242	worker	spike_loop_duration	iter=0	38555
590549992	ui	ui_fast_repeat_flush	pending_iters=1	442
590617858	worker	loop_interval_sleep	requested_ms=67	68515
590617866	worker	spike_loop_gap	after_loop=0 gap_us=68623	68623
590643550	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1453
590654857	worker	spike_loop_duration	iter=1 loop=1	36989
590655288	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	352
590655322	ui	session_end	finish success=yes loop=1
592066922	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
592069595	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3137
592071394	worker	spike_loop_gap	after_loop=1 gap_us=1416535	1416535
592071958	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
592072528	ui	refreshWorkflowEditor	elapsed_ms=2	2000
592109174	worker	spike_loop_duration	iter=0	37776
592109741	ui	ui_fast_repeat_flush	pending_iters=1	398
592185628	worker	loop_interval_sleep	requested_ms=75	76364
592185635	worker	spike_loop_gap	after_loop=0 gap_us=76461	76461
592222231	worker	spike_loop_duration	iter=1 loop=1	36594
592223079	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	746
592243144	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1033
592243170	ui	session_end	finish success=yes loop=1
597954624	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
597956996	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3020
597958766	worker	spike_loop_gap	after_loop=1 gap_us=5736533	5736533
597959449	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
597960037	ui	refreshWorkflowEditor	elapsed_ms=2	2000
597997809	worker	spike_loop_duration	iter=0	39039
597998827	ui	ui_fast_repeat_flush	pending_iters=1	656
598061091	worker	loop_interval_sleep	requested_ms=62	63127
598061098	worker	spike_loop_gap	after_loop=0 gap_us=63289	63289
598065845	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1000
598097319	worker	spike_loop_duration	iter=1 loop=1	36219
598097858	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	346
598097865	ui	session_end	finish success=yes loop=1
598145259	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
598147649	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3139
598149385	worker	spike_loop_gap	after_loop=1 gap_us=52065	52065
598149742	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
598150130	ui	refreshWorkflowEditor	elapsed_ms=1	1000
598189616	worker	spike_loop_duration	iter=0	40229
598195394	ui	ui_fast_repeat_flush	pending_iters=1	407
598237439	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1120
598242971	ui	session_end	finish success=yes
598305333	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
598309141	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4312
598310792	worker	spike_loop_gap	after_loop=0 gap_us=121173	121173
598349462	worker	spike_loop_duration	iter=0	38665
598350057	ui	ui_fast_repeat_flush	pending_iters=1	416
598403758	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1301
598411005	ui	session_end	finish success=yes
598452011	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
598454854	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3367
598456504	worker	spike_loop_gap	after_loop=0 gap_us=107041	107041
598497094	worker	spike_loop_duration	iter=0	40587
598498052	ui	ui_fast_repeat_flush	pending_iters=1	638
598547450	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1306
598550149	ui	session_end	finish success=yes
598616614	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
598619560	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3440
598621413	worker	spike_loop_gap	after_loop=0 gap_us=124318	124318
598658787	worker	spike_loop_duration	iter=0	37369
598659436	ui	ui_fast_repeat_flush	pending_iters=1	437
598711812	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1350
598722531	ui	session_end	finish success=yes
598754674	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
598757458	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3370
598759076	worker	spike_loop_gap	after_loop=0 gap_us=100289	100289
598796164	worker	spike_loop_duration	iter=0	37087
598796762	ui	ui_fast_repeat_flush	pending_iters=1	408
598867743	worker	loop_interval_sleep	requested_ms=70	71465
598867748	worker	spike_loop_gap	after_loop=0 gap_us=71583	71583
598888436	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1803
598904189	worker	spike_loop_duration	iter=1 loop=1	36438
598904784	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	504
598904816	ui	session_end	finish success=yes loop=1
606189142	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
606191883	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3189
606193832	worker	spike_loop_gap	after_loop=1 gap_us=7289641	7289641
606231634	worker	spike_loop_duration	iter=0	37797
606232587	ui	ui_fast_repeat_flush	pending_iters=1	705
606294227	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1352
606303413	ui	session_end	finish success=yes
606374612	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
606377075	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2947
606378855	worker	spike_loop_gap	after_loop=0 gap_us=147221	147221
606419112	worker	spike_loop_duration	iter=0	40254
606420059	ui	ui_fast_repeat_flush	pending_iters=1	480
606463883	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1250
606470704	ui	session_end	finish success=yes
606532332	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
606534969	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3120
606536843	worker	spike_loop_gap	after_loop=0 gap_us=117728	117728
606580253	worker	spike_loop_duration	iter=0	43406
606580978	ui	ui_fast_repeat_flush	pending_iters=1	398
606632391	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1115
606641834	ui	session_end	finish success=yes
608180129	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
608182886	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3277
608184551	worker	spike_loop_gap	after_loop=0 gap_us=1604298	1604298
608185159	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
608185592	ui	refreshWorkflowEditor	elapsed_ms=1	1000
608227944	worker	spike_loop_duration	iter=0	43390
608228586	ui	ui_fast_repeat_flush	pending_iters=1	392
608266730	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1279
608274212	ui	session_end	finish success=yes
608330121	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
608336199	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	6544
608338796	worker	spike_loop_gap	after_loop=0 gap_us=110850	110850
608378984	worker	spike_loop_duration	iter=0	40184
608379713	ui	ui_fast_repeat_flush	pending_iters=1	450
608427010	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1110
608431093	ui	session_end	finish success=yes
608491615	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
608493971	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2776
608495876	worker	spike_loop_gap	after_loop=0 gap_us=116891	116891
608538710	worker	spike_loop_duration	iter=0	42831
608539300	ui	ui_fast_repeat_flush	pending_iters=1	361
608581834	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1382
608590637	ui	session_end	finish success=yes
608655497	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
608658041	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3012
608659639	worker	spike_loop_gap	after_loop=0 gap_us=120929	120929
608699684	worker	spike_loop_duration	iter=0	40040
608700370	ui	ui_fast_repeat_flush	pending_iters=1	424
608745723	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1405
608751239	ui	session_end	finish success=yes
608795006	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
608798355	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3924
608800688	worker	spike_loop_gap	after_loop=0 gap_us=101003	101003
608843914	worker	spike_loop_duration	iter=0	43221
608845138	ui	ui_fast_repeat_flush	pending_iters=1	435
608922564	worker	loop_interval_sleep	requested_ms=77	78540
608922572	worker	spike_loop_gap	after_loop=0 gap_us=78660	78660
608938652	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1231
608959425	worker	spike_loop_duration	iter=1 loop=1	36851
608960014	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	342
608960018	ui	session_end	finish success=yes loop=1
609286516	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
609288478	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2304
609290400	worker	spike_loop_gap	after_loop=1 gap_us=330973	330973
609293367	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
609293765	ui	refreshWorkflowEditor	elapsed_ms=1	1000
609328224	worker	spike_loop_duration	iter=0	37821
609329798	ui	ui_fast_repeat_flush	pending_iters=1	840
609384210	worker	loop_interval_sleep	requested_ms=55	55871
609384214	worker	spike_loop_gap	after_loop=0 gap_us=55990	55990
609391483	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1350
609422466	worker	spike_loop_duration	iter=1 loop=1	38249
609422892	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	349
609422923	ui	session_end	finish success=yes loop=1
609456748	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
609459055	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2850
609460844	worker	spike_loop_gap	after_loop=1 gap_us=38378	38378
609502713	worker	spike_loop_duration	iter=0	41867
609507478	ui	session_end	reason=preempted_by_new_session
609507528	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
609509676	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2465
609510259	ui	ui_fast_repeat_flush	pending_iters=1	448
609516932	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
609517352	ui	refreshWorkflowEditor	elapsed_ms=1	1000
609546572	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1578
609550238	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1117
609553249	worker	spike_loop_duration	iter=0	41891
609554542	ui	ui_fast_repeat_flush	pending_iters=1	720
609554546	ui	session_end	finish success=yes
609622867	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
609625485	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3238
609627095	worker	spike_loop_gap	after_loop=0 gap_us=73843	73843
609627789	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
609628183	ui	refreshWorkflowEditor	elapsed_ms=2	2000
609631193	ui	session_end	reason=preempted_by_new_session
609631246	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
609633599	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2735
609640677	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
609641104	ui	refreshWorkflowEditor	elapsed_ms=1	1000
609665729	worker	spike_loop_duration	iter=0	38631
609665909	ui	ui_fast_repeat_flush	pending_iters=1	27
609674819	worker	spike_loop_duration	iter=0	39537
609676037	ui	ui_fast_repeat_flush	pending_iters=1	651
609714015	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	944
609716604	ui	session_end	finish success=yes
610043483	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	305267
610049459	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
610051286	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2143
610051335	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	46
610051663	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	325
610051665	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1
610052861	worker	spike_loop_gap	after_loop=0 gap_us=378041	378041
610074439	worker	spike_synthetic_key	dur_us=338351	338351
610074457	worker	spike_loop_duration	iter=1	338391
610074665	ui	ui_fast_repeat_flush	pending_iters=1	47
610074671	ui	session_end	finish success=yes
635089778	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
635092398	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3077
635094283	worker	spike_loop_gap	after_loop=1 gap_us=25019824	25019824
635132097	worker	spike_loop_duration	iter=0	37812
635136221	ui	ui_fast_repeat_flush	pending_iters=1	407
635208411	worker	loop_interval_sleep	requested_ms=75	76250
635208416	worker	spike_loop_gap	after_loop=0 gap_us=76317	76317
635245631	worker	spike_loop_duration	iter=1 loop=1	37212
635246171	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	465
635261867	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1507
635266530	ui	session_end	finish success=yes loop=1
635878502	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
635881485	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3501
635882995	worker	spike_loop_gap	after_loop=1 gap_us=637364	637364
635925485	worker	spike_loop_duration	iter=0	42486
635926486	ui	ui_fast_repeat_flush	pending_iters=1	750
636005508	worker	loop_interval_sleep	requested_ms=78	79929
636005516	worker	spike_loop_gap	after_loop=0 gap_us=80030	80030
636018918	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2180
636042707	worker	spike_loop_duration	iter=1 loop=1	37189
636043381	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	337
636043385	ui	session_end	finish success=yes loop=1
638992486	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
638997257	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	5402
638999598	worker	spike_loop_gap	after_loop=1 gap_us=2956889	2956889
639037063	worker	spike_loop_duration	iter=0	37463
639039655	ui	ui_fast_repeat_flush	pending_iters=1	429
639089566	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1107
639098615	ui	session_end	finish success=yes
639165391	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
639167702	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2804
639169582	worker	spike_loop_gap	after_loop=0 gap_us=132516	132516
639209326	worker	spike_loop_duration	iter=0	39740
639209921	ui	ui_fast_repeat_flush	pending_iters=1	422
639255956	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1180
639260938	ui	session_end	finish success=yes
639304809	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
639307438	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3345
639309027	worker	spike_loop_gap	after_loop=0 gap_us=99702	99702
639345530	worker	spike_loop_duration	iter=0	36499
639346131	ui	ui_fast_repeat_flush	pending_iters=1	428
639401908	worker	loop_interval_sleep	requested_ms=55	56284
639401916	worker	spike_loop_gap	after_loop=0 gap_us=56387	56387
639421399	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1779
639440452	worker	spike_loop_duration	iter=1 loop=1	38532
639441182	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	427
639441186	ui	session_end	finish success=yes loop=1
640904984	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
640907639	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3139
640909614	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
640909640	worker	spike_loop_gap	after_loop=1 gap_us=1469189	1469189
640910292	ui	refreshWorkflowEditor	elapsed_ms=2	2000
640947206	worker	spike_loop_duration	iter=0	37562
640947771	ui	ui_fast_repeat_flush	pending_iters=1	395
641009590	worker	loop_interval_sleep	requested_ms=61	62299
641009594	worker	spike_loop_gap	after_loop=0 gap_us=62390	62390
641013139	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1047
641045783	worker	spike_loop_duration	iter=1 loop=1	36187
641046200	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	349
641046251	ui	session_end	finish success=yes loop=1
643138352	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
643140907	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3021
643142630	worker	spike_loop_gap	after_loop=1 gap_us=2096846	2096846
643179779	worker	spike_loop_duration	iter=0	37144
643180505	ui	ui_fast_repeat_flush	pending_iters=1	454
643248792	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1047
643251822	ui	session_end	finish success=yes
643306836	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
643309413	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3079
643311061	worker	spike_loop_gap	after_loop=0 gap_us=131283	131283
643315378	ui	WorkflowEditorPanel.refresh	elapsed_ms=3	3000
643315906	ui	refreshWorkflowEditor	elapsed_ms=5	5000
643359813	worker	spike_loop_duration	iter=0	48747
643360862	ui	ui_fast_repeat_flush	pending_iters=1	765
643411614	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	903
643420643	ui	session_end	finish success=yes
643470988	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
643473493	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3008
643475339	worker	spike_loop_gap	after_loop=0 gap_us=115527	115527
643517326	worker	spike_loop_duration	iter=0	41982
643518396	ui	ui_fast_repeat_flush	pending_iters=1	578
643566752	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1103
643571542	ui	session_end	finish success=yes
643612211	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
643615496	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3811
643617740	worker	spike_loop_gap	after_loop=0 gap_us=100413	100413
643659714	worker	spike_loop_duration	iter=0	41970
643660255	ui	ui_fast_repeat_flush	pending_iters=1	366
643709389	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	893
643714030	ui	session_end	finish success=yes
643747486	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
643750625	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3922
643752266	worker	spike_loop_gap	after_loop=0 gap_us=92550	92550
643788773	worker	spike_loop_duration	iter=0	36504
643791927	ui	ui_fast_repeat_flush	pending_iters=1	985
643852586	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1010
643862129	ui	session_end	finish success=yes
644072000	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
644074659	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3190
644076342	worker	spike_loop_gap	after_loop=0 gap_us=287567	287567
644113061	worker	spike_loop_duration	iter=0	36714
644113662	ui	ui_fast_repeat_flush	pending_iters=1	384
644176587	worker	loop_interval_sleep	requested_ms=62	63406
644176594	worker	spike_loop_gap	after_loop=0 gap_us=63535	63535
644180941	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1048
644213399	worker	spike_loop_duration	iter=1 loop=1	36803
644213831	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	306
644213835	ui	session_end	finish success=yes loop=1
647825173	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
647827675	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2961
647830402	worker	spike_loop_gap	after_loop=1 gap_us=3617002	3617002
647867845	worker	spike_loop_duration	iter=0	37440
647868493	ui	ui_fast_repeat_flush	pending_iters=1	431
647913391	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1311
647919599	ui	session_end	finish success=yes
648010943	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
648013946	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3741
648015720	worker	spike_loop_gap	after_loop=0 gap_us=147873	147873
648056321	worker	spike_loop_duration	iter=0	40598
648056929	ui	ui_fast_repeat_flush	pending_iters=1	421
648102120	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	912
648108564	ui	session_end	finish success=yes
648174213	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
648176443	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2709
648178179	worker	spike_loop_gap	after_loop=0 gap_us=121857	121857
648217487	worker	spike_loop_duration	iter=0	39304
648218124	ui	ui_fast_repeat_flush	pending_iters=1	428
648268395	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1600
648273245	ui	session_end	finish success=yes
648326467	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
648329109	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3174
648330859	worker	spike_loop_gap	after_loop=0 gap_us=113371	113371
648367436	worker	spike_loop_duration	iter=0	36572
648368117	ui	ui_fast_repeat_flush	pending_iters=1	436
648433749	worker	loop_interval_sleep	requested_ms=65	66209
648433757	worker	spike_loop_gap	after_loop=0 gap_us=66322	66322
648442132	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1199
648473548	worker	spike_loop_duration	iter=1 loop=1	39788
648474022	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	383
648474053	ui	session_end	finish success=yes loop=1
649585652	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
649587960	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2688
649589464	worker	spike_loop_gap	after_loop=1 gap_us=1115917	1115917
649590147	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
649590528	ui	refreshWorkflowEditor	elapsed_ms=1	1000
649627224	worker	spike_loop_duration	iter=0	37758
649627781	ui	ui_fast_repeat_flush	pending_iters=1	390
649700833	worker	loop_interval_sleep	requested_ms=72	73518
649700839	worker	spike_loop_gap	after_loop=0 gap_us=73614	73614
649737170	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1286
649737492	worker	spike_loop_duration	iter=1 loop=1	36651
649737920	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	240
649737924	ui	session_end	finish success=yes loop=1
650187861	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
650190641	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3206
650192908	worker	spike_loop_gap	after_loop=1 gap_us=455412	455412
650193690	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
650194136	ui	refreshWorkflowEditor	elapsed_ms=1	1000
650231073	worker	spike_loop_duration	iter=0	38162
650231644	ui	ui_fast_repeat_flush	pending_iters=1	399
650310456	worker	loop_interval_sleep	requested_ms=78	79288
650310463	worker	spike_loop_gap	after_loop=0 gap_us=79390	79390
650346748	worker	spike_loop_duration	iter=1 loop=1	36283
650347262	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	460
650370773	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1171
650378052	ui	session_end	finish success=yes loop=1
653017137	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
653019370	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2717
653020922	worker	spike_loop_gap	after_loop=1 gap_us=2674172	2674172
653021458	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
653021833	ui	refreshWorkflowEditor	elapsed_ms=1	1000
653058867	worker	spike_loop_duration	iter=0	37941
653059470	ui	ui_fast_repeat_flush	pending_iters=1	398
653110179	worker	loop_interval_sleep	requested_ms=50	51205
653110185	worker	spike_loop_gap	after_loop=0 gap_us=51320	51320
653147625	worker	spike_loop_duration	iter=1 loop=1	37438
653148099	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	406
653224262	worker	loop_interval_sleep	requested_ms=75 loop=1	76502
653224270	worker	spike_loop_gap	after_loop=1 gap_us=76643 loop=1	76643
653271650	worker	spike_loop_duration	iter=2 loop=2	47376
653272027	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	311
653339424	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=2	1100
653343557	ui	session_end	finish success=yes loop=2
657395583	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
657398438	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3605
657400100	worker	spike_loop_gap	after_loop=2 gap_us=4128451	4128451
657437692	worker	spike_loop_duration	iter=0	37588
657438528	ui	ui_fast_repeat_flush	pending_iters=1	433
657510304	worker	loop_interval_sleep	requested_ms=71	72279
657510310	worker	spike_loop_gap	after_loop=0 gap_us=72619	72619
657527865	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1402
657546674	worker	spike_loop_duration	iter=1 loop=1	36361
657547072	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	336
657547104	ui	session_end	finish success=yes loop=1
657638746	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
657640799	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2402
657642619	worker	spike_loop_gap	after_loop=1 gap_us=95945	95945
657686557	worker	spike_loop_duration	iter=0	43936
657687619	ui	ui_fast_repeat_flush	pending_iters=1	423
657737522	worker	loop_interval_sleep	requested_ms=50	50913
657737527	worker	spike_loop_gap	after_loop=0 gap_us=50970	50970
657741942	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1120
657773733	worker	spike_loop_duration	iter=1 loop=1	36203
657774093	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	306
657774121	ui	session_end	finish success=yes loop=1
657969495	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
657972531	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3593
657974032	worker	spike_loop_gap	after_loop=1 gap_us=200298	200298
658013467	worker	spike_loop_duration	iter=0	39433
658014211	ui	ui_fast_repeat_flush	pending_iters=1	406
658069816	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1369
658073726	ui	session_end	finish success=yes
666843529	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
666846117	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3005
666847934	worker	spike_loop_gap	after_loop=0 gap_us=8834465	8834465
666848289	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
666848923	ui	refreshWorkflowEditor	elapsed_ms=2	2000
666886881	worker	spike_loop_duration	iter=0	38943
666887437	ui	ui_fast_repeat_flush	pending_iters=1	404
666949758	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1349
666950105	ui	session_end	finish success=yes
674235992	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
674237767	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2093
674239303	worker	spike_loop_gap	after_loop=0 gap_us=7352422	7352422
674239791	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
674240322	ui	refreshWorkflowEditor	elapsed_ms=2	2000
674277951	worker	spike_loop_duration	iter=0	38646
674278599	ui	ui_fast_repeat_flush	pending_iters=1	467
674339400	worker	loop_interval_sleep	requested_ms=60	61358
674339407	worker	spike_loop_gap	after_loop=0 gap_us=61454	61454
674376619	worker	spike_loop_duration	iter=1 loop=1	37209
674377299	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	448
674414396	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1264
674417687	ui	session_end	finish success=yes loop=1
675766480	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
675768756	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2772
675770134	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
675771007	worker	spike_loop_gap	after_loop=1 gap_us=1394387	1394387
675771432	ui	refreshWorkflowEditor	elapsed_ms=2	2000
675808227	worker	spike_loop_duration	iter=0	37216
675808835	ui	ui_fast_repeat_flush	pending_iters=1	432
675867728	worker	loop_interval_sleep	requested_ms=58	59373
675867732	worker	spike_loop_gap	after_loop=0 gap_us=59507	59507
675875332	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1326
675908299	worker	spike_loop_duration	iter=1 loop=1	40564
675909044	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	325
675909049	ui	session_end	finish success=yes loop=1
678486185	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
678488109	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2304
678490410	worker	spike_loop_gap	after_loop=1 gap_us=2582111	2582111
678491060	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
678491443	ui	refreshWorkflowEditor	elapsed_ms=2	2000
678532052	worker	spike_loop_duration	iter=0	41637
678532900	ui	ui_fast_repeat_flush	pending_iters=1	431
678598047	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1124
678600891	ui	session_end	finish success=yes
678688008	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
678690437	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2945
678692188	worker	spike_loop_gap	after_loop=0 gap_us=160137	160137
678729038	worker	spike_loop_duration	iter=0	36845
678729771	ui	ui_fast_repeat_flush	pending_iters=1	434
678785487	worker	loop_interval_sleep	requested_ms=55	56338
678785494	worker	spike_loop_gap	after_loop=0 gap_us=56458	56458
678793460	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1299
678823772	worker	spike_loop_duration	iter=1 loop=1	38275
678824219	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	370
678824342	ui	session_end	finish success=yes loop=1
680359480	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
680362150	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3303
680363970	worker	spike_loop_gap	after_loop=1 gap_us=1540197	1540197
680364699	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
680365158	ui	refreshWorkflowEditor	elapsed_ms=2	2000
680402101	worker	spike_loop_duration	iter=0	38127
680402837	ui	ui_fast_repeat_flush	pending_iters=1	430
680464699	worker	loop_interval_sleep	requested_ms=61	62475
680464704	worker	spike_loop_gap	after_loop=0 gap_us=62604	62604
680502278	worker	spike_loop_duration	iter=1 loop=1	37571
680502702	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	338
680559845	worker	loop_interval_sleep	requested_ms=56 loop=1	57424
680559851	worker	spike_loop_gap	after_loop=1 gap_us=57573 loop=1	57573
680616390	worker	spike_loop_duration	iter=2 loop=2	56536
680616791	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	327
680621268	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=2	1265
680627032	ui	session_end	finish success=yes loop=2
686429954	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
686432391	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2948
686434421	worker	spike_loop_gap	after_loop=2 gap_us=5818030	5818030
686435249	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
686435857	ui	refreshWorkflowEditor	elapsed_ms=2	2000
686477190	worker	spike_loop_duration	iter=0	42764
686478018	ui	ui_fast_repeat_flush	pending_iters=1	434
686528414	worker	loop_interval_sleep	requested_ms=50	51123
686528419	worker	spike_loop_gap	after_loop=0 gap_us=51231	51231
686558885	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1555
686565691	worker	spike_loop_duration	iter=1 loop=1	37268
686566107	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	326
686566137	ui	session_end	finish success=yes loop=1
688364925	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
688367681	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3234
688369359	worker	spike_loop_gap	after_loop=1 gap_us=1803666	1803666
688369979	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
688370422	ui	refreshWorkflowEditor	elapsed_ms=2	2000
688407295	worker	spike_loop_duration	iter=0	37931
688407976	ui	ui_fast_repeat_flush	pending_iters=1	495
688460628	worker	loop_interval_sleep	requested_ms=52	53233
688460633	worker	spike_loop_gap	after_loop=0 gap_us=53340	53340
688471466	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	993
688502909	worker	spike_loop_duration	iter=1 loop=1	42273
688503793	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	780
688504214	ui	session_end	finish success=yes loop=1
690652755	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
690655346	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3300
690656971	worker	spike_loop_gap	after_loop=1 gap_us=2154062	2154062
690657597	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
690658242	ui	refreshWorkflowEditor	elapsed_ms=2	2000
690695100	worker	spike_loop_duration	iter=0	38125
690695756	ui	ui_fast_repeat_flush	pending_iters=1	495
690745047	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1729
690749677	ui	session_end	finish success=yes
690827446	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
690830256	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3323
690832045	worker	spike_loop_gap	after_loop=0 gap_us=136943	136943
690869643	worker	spike_loop_duration	iter=0	37594
690870346	ui	ui_fast_repeat_flush	pending_iters=1	505
690920964	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1063
690921233	ui	session_end	finish success=yes
692296787	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
692298684	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2879
692300423	worker	spike_loop_gap	after_loop=0 gap_us=1430780	1430780
692301459	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
692301840	ui	refreshWorkflowEditor	elapsed_ms=1	1000
692338975	worker	spike_loop_duration	iter=0	38551
692339607	ui	ui_fast_repeat_flush	pending_iters=1	344
692413555	worker	loop_interval_sleep	requested_ms=73	74528
692413562	worker	spike_loop_gap	after_loop=0 gap_us=74586	74586
692449727	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1337
692450117	worker	spike_loop_duration	iter=1 loop=1	36553
692450598	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	238
692450602	ui	session_end	finish success=yes loop=1
705164262	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
705166515	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2719
705168284	worker	spike_loop_gap	after_loop=1 gap_us=12718165	12718165
705168979	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
705169622	ui	refreshWorkflowEditor	elapsed_ms=2	2000
705208221	worker	spike_loop_duration	iter=0	39933
705209187	ui	ui_fast_repeat_flush	pending_iters=1	440
705276895	worker	loop_interval_sleep	requested_ms=67	68580
705276901	worker	spike_loop_gap	after_loop=0 gap_us=68681	68681
705313304	worker	spike_loop_duration	iter=1 loop=1	36401
705313954	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	594
705317947	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1254
705323592	ui	session_end	finish success=yes loop=1
707315334	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
707318282	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3686
707319965	worker	spike_loop_gap	after_loop=1 gap_us=2006659	2006659
707320697	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
707321076	ui	refreshWorkflowEditor	elapsed_ms=2	2000
707357790	worker	spike_loop_duration	iter=0	37823
707358670	ui	ui_fast_repeat_flush	pending_iters=1	701
707415953	worker	loop_interval_sleep	requested_ms=57	58081
707415959	worker	spike_loop_gap	after_loop=0 gap_us=58168	58168
707458225	worker	spike_loop_duration	iter=1 loop=1	42263
707458655	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	344
707507734	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1027
707510759	ui	session_end	finish success=yes loop=1
709587872	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
709589701	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2219
709591449	worker	spike_loop_gap	after_loop=1 gap_us=2133223	2133223
709592048	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
709592495	ui	refreshWorkflowEditor	elapsed_ms=2	2000
709629534	worker	spike_loop_duration	iter=0	38081
709630321	ui	ui_fast_repeat_flush	pending_iters=1	605
709694963	worker	loop_interval_sleep	requested_ms=64	65325
709694969	worker	spike_loop_gap	after_loop=0 gap_us=65437	65437
709705486	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1188
709731159	worker	spike_loop_duration	iter=1 loop=1	36187
709731517	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	306
709731547	ui	session_end	finish success=yes loop=1
709817280	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
709820734	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3828
709822745	worker	spike_loop_gap	after_loop=1 gap_us=91582	91582
709861331	worker	spike_loop_duration	iter=0	38580
709862005	ui	ui_fast_repeat_flush	pending_iters=1	420
709868521	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1373
709871793	ui	session_end	finish success=yes
709943366	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
709945471	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2483
709947106	worker	spike_loop_gap	after_loop=0 gap_us=85775	85775
709987334	worker	spike_loop_duration	iter=0	40225
709988003	ui	ui_fast_repeat_flush	pending_iters=1	431
710061961	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1283
710069763	ui	session_end	finish success=yes
714227615	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
714230359	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3218
714233123	worker	spike_loop_gap	after_loop=0 gap_us=4245787	4245787
714270296	worker	spike_loop_duration	iter=0	37170
714271233	ui	ui_fast_repeat_flush	pending_iters=1	531
714321814	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1088
714325902	ui	session_end	finish success=yes
714403434	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
714406419	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3440
714408022	worker	spike_loop_gap	after_loop=0 gap_us=137726	137726
714447683	worker	spike_loop_duration	iter=0	39657
714448290	ui	ui_fast_repeat_flush	pending_iters=1	416
714498098	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1008
714503079	ui	session_end	finish success=yes
714567955	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
714571244	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3780
714572990	worker	spike_loop_gap	after_loop=0 gap_us=125306	125306
714613130	worker	spike_loop_duration	iter=0	40134
714613876	ui	ui_fast_repeat_flush	pending_iters=1	406
714665110	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	992
714676398	ui	session_end	finish success=yes
732740818	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
732742802	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2355
732744352	worker	spike_loop_gap	after_loop=0 gap_us=18131223	18131223
732782149	worker	spike_loop_duration	iter=0	37794
732787116	ui	ui_fast_repeat_flush	pending_iters=1	424
732842224	worker	loop_interval_sleep	requested_ms=59	59965
732842230	worker	spike_loop_gap	after_loop=0 gap_us=60080	60080
732879287	worker	spike_loop_duration	iter=1 loop=1	37055
732879807	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	374
732898678	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1170
732900837	ui	session_end	finish success=yes loop=1
733028540	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
733031080	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3019
733033205	worker	spike_loop_gap	after_loop=1 gap_us=153916	153916
733073815	worker	spike_loop_duration	iter=0	40606
733074850	ui	ui_fast_repeat_flush	pending_iters=1	424
733139381	worker	loop_interval_sleep	requested_ms=64	65324
733139386	worker	spike_loop_gap	after_loop=0 gap_us=65572	65572
733159628	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1171
733176144	worker	spike_loop_duration	iter=1 loop=1	36755
733176917	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	733
733177060	ui	session_end	finish success=yes loop=1
736886039	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
736887762	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2296
736889925	worker	spike_loop_gap	after_loop=1 gap_us=3713781	3713781
736929910	worker	spike_loop_duration	iter=0	39983
736930495	ui	ui_fast_repeat_flush	pending_iters=1	412
736973647	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1002
736981288	ui	session_end	finish success=yes
737069139	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
737071649	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2990
737073273	worker	spike_loop_gap	after_loop=0 gap_us=143361	143361
737110954	worker	spike_loop_duration	iter=0	37676
737111530	ui	ui_fast_repeat_flush	pending_iters=1	406
737162431	worker	loop_interval_sleep	requested_ms=50	51371
737162439	worker	spike_loop_gap	after_loop=0 gap_us=51486	51486
737175024	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1318
737203806	worker	spike_loop_duration	iter=1 loop=1	41364
737204292	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	380
737204330	ui	session_end	finish success=yes loop=1
737235969	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
737237917	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2263
737239451	worker	spike_loop_gap	after_loop=1 gap_us=35642	35642
737285477	worker	spike_loop_duration	iter=0	46024
737291062	ui	ui_fast_repeat_flush	pending_iters=1	399
737341765	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1086
737346993	ui	session_end	finish success=yes
744487281	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
744489481	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2613
744491270	worker	spike_loop_gap	after_loop=0 gap_us=7205792	7205792
744491649	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
744492531	ui	refreshWorkflowEditor	elapsed_ms=2	2000
744529958	worker	spike_loop_duration	iter=0	38686
744530581	ui	ui_fast_repeat_flush	pending_iters=1	416
744593032	worker	loop_interval_sleep	requested_ms=62	63007
744593038	worker	spike_loop_gap	after_loop=0 gap_us=63079	63079
744607815	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1321
744629465	worker	spike_loop_duration	iter=1 loop=1	36357
744630186	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	328
744630190	ui	session_end	finish success=yes loop=1
746551110	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
746553699	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3166
746555336	worker	spike_loop_gap	after_loop=1 gap_us=1925871	1925871
746555723	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
746556106	ui	refreshWorkflowEditor	elapsed_ms=1	1000
746593053	worker	spike_loop_duration	iter=0	37714
746593680	ui	ui_fast_repeat_flush	pending_iters=1	429
746652986	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1215
746657339	ui	session_end	finish success=yes
752391967	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
752394131	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2621
752395862	worker	spike_loop_gap	after_loop=0 gap_us=5802808	5802808
752396183	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
752396727	ui	refreshWorkflowEditor	elapsed_ms=2	2000
752435668	worker	spike_loop_duration	iter=0	39805
752436492	ui	ui_fast_repeat_flush	pending_iters=1	539
752513255	worker	loop_interval_sleep	requested_ms=76	77541
752513262	worker	spike_loop_gap	after_loop=0 gap_us=77593	77593
752520565	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1063
752549952	worker	spike_loop_duration	iter=1 loop=1	36688
752550376	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	357
752550964	ui	session_end	finish success=yes loop=1
755260773	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
755263220	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2948
755264882	worker	spike_loop_gap	after_loop=1 gap_us=2714930	2714930
755304725	worker	spike_loop_duration	iter=0	39839
755305779	ui	ui_fast_repeat_flush	pending_iters=1	805
755370246	worker	loop_interval_sleep	requested_ms=64	65420
755370253	worker	spike_loop_gap	after_loop=0 gap_us=65529	65529
755392913	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1537
755407324	worker	spike_loop_duration	iter=1 loop=1	37068
755407732	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	346
755407930	ui	session_end	finish success=yes loop=1
756414496	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
756417446	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3436
756419447	worker	spike_loop_gap	after_loop=1 gap_us=1012123	1012123
756419954	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
756420434	ui	refreshWorkflowEditor	elapsed_ms=2	2000
756457380	worker	spike_loop_duration	iter=0	37928
756457963	ui	ui_fast_repeat_flush	pending_iters=1	412
756510987	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	956
756516397	ui	session_end	finish success=yes
756591839	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
756594755	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3383
756596345	worker	spike_loop_gap	after_loop=0 gap_us=138965	138965
756637373	worker	spike_loop_duration	iter=0	41024
756638132	ui	ui_fast_repeat_flush	pending_iters=1	424
756682550	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	940
756689158	ui	session_end	finish success=yes
761130835	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
761134226	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3884
761135819	worker	spike_loop_gap	after_loop=0 gap_us=4498445	4498445
761177142	worker	spike_loop_duration	iter=0	41317
761177876	ui	ui_fast_repeat_flush	pending_iters=1	348
761229366	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1014
761229519	ui	session_end	finish success=yes
761324761	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
761328191	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3944
761329926	worker	spike_loop_gap	after_loop=0 gap_us=152786	152786
761366514	worker	spike_loop_duration	iter=0	36583
761367669	ui	ui_fast_repeat_flush	pending_iters=1	608
761419683	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1368
761428425	ui	session_end	finish success=yes
761486902	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
761489034	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2499
761490692	worker	spike_loop_gap	after_loop=0 gap_us=124178	124178
761530708	worker	spike_loop_duration	iter=0	40014
761531666	ui	ui_fast_repeat_flush	pending_iters=1	689
761589906	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1337
761594403	ui	session_end	finish success=yes
761653899	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
761657129	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4178
761659509	worker	spike_loop_gap	after_loop=0 gap_us=128798	128798
761696653	worker	spike_loop_duration	iter=0	37141
761697266	ui	ui_fast_repeat_flush	pending_iters=1	411
761766001	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1410
761770667	ui	session_end	finish success=yes
762570339	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
762573100	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3226
762574612	worker	spike_loop_gap	after_loop=0 gap_us=877958	877958
762575267	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
762575698	ui	refreshWorkflowEditor	elapsed_ms=2	2000
762612471	worker	spike_loop_duration	iter=0	37855
762613108	ui	ui_fast_repeat_flush	pending_iters=1	397
762676150	worker	loop_interval_sleep	requested_ms=62	63584
762676159	worker	spike_loop_gap	after_loop=0 gap_us=63688	63688
762712727	worker	spike_loop_duration	iter=1 loop=1	36566
762713171	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	384
762714240	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	730
762722978	ui	session_end	finish success=yes loop=1
765195040	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
765198124	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3759
765200304	worker	spike_loop_gap	after_loop=1 gap_us=2487577	2487577
765201114	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
765201564	ui	refreshWorkflowEditor	elapsed_ms=2	2000
765238300	worker	spike_loop_duration	iter=0	37992
765239435	ui	ui_fast_repeat_flush	pending_iters=1	438
765290877	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	975
765299914	ui	session_end	finish success=yes
765360963	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
765363347	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2973
765364889	worker	spike_loop_gap	after_loop=0 gap_us=126587	126587
765404244	worker	spike_loop_duration	iter=0	39351
765405197	ui	ui_fast_repeat_flush	pending_iters=1	689
765459592	worker	loop_interval_sleep	requested_ms=54	55244
765459599	worker	spike_loop_gap	after_loop=0 gap_us=55356	55356
765466746	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1179
765497943	worker	spike_loop_duration	iter=1 loop=1	38341
765498394	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	358
765498429	ui	session_end	finish success=yes loop=1
765529114	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
765531776	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3444
765533792	worker	spike_loop_gap	after_loop=1 gap_us=35849	35849
765583109	worker	spike_loop_duration	iter=0	49315
765587431	ui	ui_fast_repeat_flush	pending_iters=1	650
765597743	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1252
765603871	ui	session_end	finish success=yes
765678072	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
765680653	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3029
765682346	worker	spike_loop_gap	after_loop=0 gap_us=99234	99234
765721673	worker	spike_loop_duration	iter=0	39322
765722268	ui	ui_fast_repeat_flush	pending_iters=1	397
765787708	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	944
765793423	ui	session_end	finish success=yes
771004466	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
771007510	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3662
771009583	worker	spike_loop_gap	after_loop=0 gap_us=5287909	5287909
771046527	worker	spike_loop_duration	iter=0	36939
771047251	ui	ui_fast_repeat_flush	pending_iters=1	401
771102849	worker	loop_interval_sleep	requested_ms=55	56232
771102856	worker	spike_loop_gap	after_loop=0 gap_us=56330	56330
771112816	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1028
771143333	worker	spike_loop_duration	iter=1 loop=1	40474
771143787	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	390
771143820	ui	session_end	finish success=yes loop=1
771189215	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
771191830	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3081
771193559	worker	spike_loop_gap	after_loop=1 gap_us=50225	50225
771260914	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1357
771263944	worker	spike_loop_duration	iter=0	70383
771264539	ui	ui_fast_repeat_flush	pending_iters=1	391
771264541	ui	session_end	finish success=yes
771336809	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
771338786	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2288
771340472	worker	spike_loop_gap	after_loop=0 gap_us=76526	76526
771379372	worker	spike_loop_duration	iter=0	38895
771380493	ui	ui_fast_repeat_flush	pending_iters=1	570
771433998	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2746
771442194	ui	session_end	finish success=yes
773136325	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
773138118	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2097
773139833	worker	spike_loop_gap	after_loop=0 gap_us=1760461	1760461
773140331	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
773140781	ui	refreshWorkflowEditor	elapsed_ms=2	2000
773177474	worker	spike_loop_duration	iter=0	37637
773178236	ui	ui_fast_repeat_flush	pending_iters=1	418
773232975	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	923
773239486	ui	session_end	finish success=yes
773298667	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
773301750	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3598
773303207	worker	spike_loop_gap	after_loop=0 gap_us=125734	125734
773343051	worker	spike_loop_duration	iter=0	39842
773343767	ui	ui_fast_repeat_flush	pending_iters=1	499
773402412	worker	loop_interval_sleep	requested_ms=58	59249
773402416	worker	spike_loop_gap	after_loop=0 gap_us=59365	59365
773705848	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	302543
773717725	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	53
773717728	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	2
773717757	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	27
773736558	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	6
773739016	worker	spike_synthetic_key	dur_us=336586 loop=1	336586
773739028	worker	spike_loop_duration	iter=1 loop=1	336610
773740007	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	477
773740012	ui	session_end	finish success=yes loop=1
774014761	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
774018009	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3746
774019949	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
774021185	worker	spike_loop_gap	after_loop=1 gap_us=282155	282155
774021942	ui	refreshWorkflowEditor	elapsed_ms=3	3000
774060024	worker	spike_loop_duration	iter=0	38835
774060646	ui	ui_fast_repeat_flush	pending_iters=1	417
774126402	worker	loop_interval_sleep	requested_ms=65	66285
774126409	worker	spike_loop_gap	after_loop=0 gap_us=66385	66385
774153664	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1523
774163341	worker	spike_loop_duration	iter=1 loop=1	36930
774163907	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	499
774164064	ui	session_end	finish success=yes loop=1
775845937	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
775848437	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3336
775850429	worker	spike_loop_gap	after_loop=1 gap_us=1687086	1687086
775851100	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
775851681	ui	refreshWorkflowEditor	elapsed_ms=2	2000
775888630	worker	spike_loop_duration	iter=0	38197
775889210	ui	ui_fast_repeat_flush	pending_iters=1	433
775952241	worker	loop_interval_sleep	requested_ms=62	63530
775952247	worker	spike_loop_gap	after_loop=0 gap_us=63618	63618
775955022	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	766
775989806	worker	spike_loop_duration	iter=1 loop=1	37557
775990194	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	335
775990421	ui	session_end	finish success=yes loop=1
776090536	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
776092412	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2199
776094070	worker	spike_loop_gap	after_loop=1 gap_us=104260	104260
776099861	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
776100384	ui	refreshWorkflowEditor	elapsed_ms=1	1000
776136846	worker	spike_loop_duration	iter=0	42774
776137643	ui	ui_fast_repeat_flush	pending_iters=1	393
776159867	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1569
776201121	ui	session_end	finish success=yes
776239825	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
776241642	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2124
776243325	worker	spike_loop_gap	after_loop=0 gap_us=106476	106476
776281136	worker	spike_loop_duration	iter=0	37807
776281838	ui	ui_fast_repeat_flush	pending_iters=1	525
776342877	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1003
776347231	ui	session_end	finish success=yes
776379531	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
776382838	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3773
776384936	worker	spike_loop_gap	after_loop=0 gap_us=103798	103798
776423049	worker	spike_loop_duration	iter=0	38107
776423660	ui	ui_fast_repeat_flush	pending_iters=1	407
776478594	worker	loop_interval_sleep	requested_ms=54	55388
776478604	worker	spike_loop_gap	after_loop=0 gap_us=55557	55557
776484540	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1697
776501044	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	147
776515467	worker	spike_loop_duration	iter=1 loop=1	36861
776516381	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	769
776516617	ui	session_end	finish success=yes loop=1
776607114	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4
778786296	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
778788054	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2108
778789877	worker	spike_loop_gap	after_loop=1 gap_us=2274410	2274410
778827278	worker	spike_loop_duration	iter=0	37397
778827969	ui	ui_fast_repeat_flush	pending_iters=1	389
778888870	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	954
778898723	ui	session_end	finish success=yes
778962442	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
778965126	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3150
778966633	worker	spike_loop_gap	after_loop=0 gap_us=139354	139354
779006165	worker	spike_loop_duration	iter=0	39529
779006709	ui	ui_fast_repeat_flush	pending_iters=1	396
779063608	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1739
779068755	ui	session_end	finish success=yes
779117136	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
779119358	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2887
779121080	worker	spike_loop_gap	after_loop=0 gap_us=114914	114914
779160904	worker	spike_loop_duration	iter=0	39819
779161495	ui	ui_fast_repeat_flush	pending_iters=1	340
779212932	worker	loop_interval_sleep	requested_ms=51	51958
779212938	worker	spike_loop_gap	after_loop=0 gap_us=52035	52035
779517068	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	302236
779533381	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	90
779533388	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2
779533442	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	51
779533445	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2
779549357	worker	spike_synthetic_key	dur_us=336401 loop=1	336401
779549372	worker	spike_loop_duration	iter=1 loop=1	336433
779549904	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	455
779551002	ui	session_end	finish success=yes loop=1
779582770	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
779585520	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3266
779587473	worker	spike_loop_gap	after_loop=1 gap_us=38097	38097
779634411	worker	spike_loop_duration	iter=0	46936
779639273	ui	ui_fast_repeat_flush	pending_iters=1	431
779677279	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1073
779686066	ui	session_end	finish success=yes
779750823	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
779752738	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2342
779754521	worker	spike_loop_gap	after_loop=0 gap_us=120104	120104
779795026	worker	spike_loop_duration	iter=0	40501
779795570	ui	ui_fast_repeat_flush	pending_iters=1	390
779849380	worker	loop_interval_sleep	requested_ms=53	54241
779849392	worker	spike_loop_gap	after_loop=0 gap_us=54365	54365
779859373	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1992
779889185	worker	spike_loop_duration	iter=1 loop=1	39790
779889587	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	346
779889619	ui	session_end	finish success=yes loop=1
779920942	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
779923705	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3474
779926221	worker	spike_loop_gap	after_loop=1 gap_us=37036	37036
779978523	worker	spike_loop_duration	iter=0	52300
779980682	ui	session_end	finish success=yes
780012557	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	33104
780254683	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
780261200	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	7351
780261615	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	406
780263146	worker	spike_loop_gap	after_loop=0 gap_us=284620	284620
780306267	worker	spike_loop_duration	iter=0	43116
780309555	ui	ui_fast_repeat_flush	pending_iters=1	941
780309563	ui	session_end	finish success=yes
785672569	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
785675328	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3347
785676982	worker	spike_loop_gap	after_loop=0 gap_us=5370715	5370715
785677519	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
785678101	ui	refreshWorkflowEditor	elapsed_ms=2	2000
785715723	worker	spike_loop_duration	iter=0	38739
785716356	ui	ui_fast_repeat_flush	pending_iters=1	435
785776613	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1247
785782233	ui	session_end	finish success=yes
785950034	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
785952678	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3374
785954170	worker	spike_loop_gap	after_loop=0 gap_us=238445	238445
785954625	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
785955025	ui	refreshWorkflowEditor	elapsed_ms=1	1000
785992107	worker	spike_loop_duration	iter=0	37933
785993300	ui	ui_fast_repeat_flush	pending_iters=1	747
786045701	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1000
786051335	ui	session_end	finish success=yes
788914703	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
788917523	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3384
788919378	worker	spike_loop_gap	after_loop=0 gap_us=2927271	2927271
788919952	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
788920345	ui	refreshWorkflowEditor	elapsed_ms=1	1000
788957536	worker	spike_loop_duration	iter=0	38156
788958164	ui	ui_fast_repeat_flush	pending_iters=1	437
789012685	worker	loop_interval_sleep	requested_ms=54	55053
789012691	worker	spike_loop_gap	after_loop=0 gap_us=55155	55155
789018319	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	902
789049336	worker	spike_loop_duration	iter=1 loop=1	36642
789050178	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	698
789050383	ui	session_end	finish success=yes loop=1
789092424	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
789094294	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2239
789095725	worker	spike_loop_gap	after_loop=1 gap_us=46390	46390
789152114	worker	spike_loop_duration	iter=0	56387
789157372	ui	ui_fast_repeat_flush	pending_iters=1	594
789181988	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1165
789188660	ui	session_end	finish success=yes
789263981	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
789266325	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2681
789268238	worker	spike_loop_gap	after_loop=0 gap_us=116119	116119
789310053	worker	spike_loop_duration	iter=0	41811
789311096	ui	ui_fast_repeat_flush	pending_iters=1	696
789373714	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	841
789378114	ui	session_end	finish success=yes
789436410	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
789438663	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2583
789440612	worker	spike_loop_gap	after_loop=0 gap_us=130560	130560
789480285	worker	spike_loop_duration	iter=0	39669
789480936	ui	ui_fast_repeat_flush	pending_iters=1	409
789531208	worker	loop_interval_sleep	requested_ms=50	50829
789531215	worker	spike_loop_gap	after_loop=0 gap_us=50932	50932
789567993	worker	spike_loop_duration	iter=1 loop=1	36775
789568737	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	632
789581455	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1006
789588786	ui	session_end	finish success=yes loop=1
789668946	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
789670871	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2249
789672467	worker	spike_loop_gap	after_loop=1 gap_us=104473	104473
789710263	worker	spike_loop_duration	iter=0	37791
789710961	ui	ui_fast_repeat_flush	pending_iters=1	376
789761528	worker	loop_interval_sleep	requested_ms=50	51195
789761537	worker	spike_loop_gap	after_loop=0 gap_us=51276	51276
789790197	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1279
789811092	worker	spike_loop_duration	iter=1 loop=1	49552
789811597	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	424
789811633	ui	session_end	finish success=yes loop=1
789848661	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
789851246	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3218
789852920	worker	spike_loop_gap	after_loop=1 gap_us=41826	41826
789898236	worker	spike_loop_duration	iter=0	45314
789903662	ui	ui_fast_repeat_flush	pending_iters=1	365
789971675	worker	loop_interval_sleep	requested_ms=72	73380
789971683	worker	spike_loop_gap	after_loop=0 gap_us=73445	73445
789997773	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1037
790028623	worker	spike_loop_duration	iter=1 loop=1	56938
790029043	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	355
790029072	ui	session_end	finish success=yes loop=1
796397707	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
796400549	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3370
796402135	worker	spike_loop_gap	after_loop=1 gap_us=6373511	6373511
796402521	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
796403122	ui	refreshWorkflowEditor	elapsed_ms=2	2000
796440178	worker	spike_loop_duration	iter=0	38039
796440766	ui	ui_fast_repeat_flush	pending_iters=1	424
796500257	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1736
796507853	ui	session_end	finish success=yes
796574257	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
796576917	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3197
796578390	worker	spike_loop_gap	after_loop=0 gap_us=138213	138213
796618219	worker	spike_loop_duration	iter=0	39824
796619022	ui	ui_fast_repeat_flush	pending_iters=1	430
796671907	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	887
796679758	ui	session_end	finish success=yes
796716126	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
796718678	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2987
796720785	worker	spike_loop_gap	after_loop=0 gap_us=102567	102567
796757780	worker	spike_loop_duration	iter=0	36991
796758397	ui	ui_fast_repeat_flush	pending_iters=1	446
796832538	worker	loop_interval_sleep	requested_ms=73	74666
796832544	worker	spike_loop_gap	after_loop=0 gap_us=74765	74765
796840186	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1406
796873952	worker	spike_loop_duration	iter=1 loop=1	41405
796874609	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	563
796874646	ui	session_end	finish success=yes loop=1
797093300	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
797096101	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3543
797098220	worker	spike_loop_gap	after_loop=1 gap_us=224267	224267
797099210	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
797100003	ui	refreshWorkflowEditor	elapsed_ms=3	3000
797138068	worker	spike_loop_duration	iter=0	39845
797138858	ui	ui_fast_repeat_flush	pending_iters=1	434
797202196	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1010
797208696	ui	session_end	finish success=yes
797294727	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
797297444	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3183
797299363	worker	spike_loop_gap	after_loop=0 gap_us=161292	161292
797300091	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
797300734	ui	refreshWorkflowEditor	elapsed_ms=2	2000
797338928	worker	spike_loop_duration	iter=0	39564
797339728	ui	ui_fast_repeat_flush	pending_iters=1	415
797396155	worker	loop_interval_sleep	requested_ms=56	57178
797396164	worker	spike_loop_gap	after_loop=0 gap_us=57234	57234
797406508	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	930
797437508	worker	spike_loop_duration	iter=1 loop=1	41341
797438249	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	681
797438299	ui	session_end	finish success=yes loop=1
797470441	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
797473096	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3124
797475063	worker	spike_loop_gap	after_loop=1 gap_us=37553	37553
797524084	worker	spike_loop_duration	iter=0	49018
797527546	ui	ui_fast_repeat_flush	pending_iters=1	612
797562284	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1186
797564898	ui	session_end	finish success=yes
797633274	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
797635067	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2111
797637529	worker	spike_loop_gap	after_loop=0 gap_us=113443	113443
797677866	worker	spike_loop_duration	iter=0	40333
797678723	ui	ui_fast_repeat_flush	pending_iters=1	499
797745093	worker	loop_interval_sleep	requested_ms=66	67134
797745098	worker	spike_loop_gap	after_loop=0 gap_us=67233	67233
798054776	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	306407
798069872	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	206
798069880	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	3
798069928	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	47
798069931	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	2
798082238	worker	spike_synthetic_key	dur_us=337120 loop=1	337120
798082252	worker	spike_loop_duration	iter=1 loop=1	337152
798091395	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	639
798091404	ui	session_end	finish success=yes loop=1
798120978	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
798123637	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3056
798125431	worker	spike_loop_gap	after_loop=1 gap_us=43175	43175
798180324	worker	spike_loop_duration	iter=0	54888
798181288	ui	ui_fast_repeat_flush	pending_iters=1	440
798205096	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1111
798211170	ui	session_end	finish success=yes
798282589	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
798285391	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3259
798286963	worker	spike_loop_gap	after_loop=0 gap_us=106638	106638
798327410	worker	spike_loop_duration	iter=0	40443
798328016	ui	ui_fast_repeat_flush	pending_iters=1	388
798391192	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1387
798395851	ui	session_end	finish success=yes
798427640	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
798430333	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3124
798431967	worker	spike_loop_gap	after_loop=0 gap_us=104554	104554
798436185	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
798436581	ui	refreshWorkflowEditor	elapsed_ms=1	1000
798470734	worker	spike_loop_duration	iter=0	38763
798471646	ui	ui_fast_repeat_flush	pending_iters=1	659
798527510	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1642
798532032	ui	session_end	finish success=yes
798572712	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
798574970	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2804
798576542	worker	spike_loop_gap	after_loop=0 gap_us=105807	105807
798616925	worker	spike_loop_duration	iter=0	40380
798617626	ui	ui_fast_repeat_flush	pending_iters=1	458
798677566	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1333
798679818	ui	session_end	finish success=yes
800948646	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
800951407	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3301
800953579	worker	spike_loop_gap	after_loop=0 gap_us=2336652	2336652
800992844	worker	spike_loop_duration	iter=0	39260
800993473	ui	ui_fast_repeat_flush	pending_iters=1	417
801059143	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1431
801064697	ui	session_end	finish success=yes
816546025	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
816549158	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3630
816551816	worker	spike_loop_gap	after_loop=0 gap_us=15558971	15558971
816592445	worker	spike_loop_duration	iter=0	40623
816593097	ui	ui_fast_repeat_flush	pending_iters=1	490
816644934	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1139
816653915	ui	session_end	finish success=yes
816902258	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
816905088	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3326
816906680	worker	spike_loop_gap	after_loop=0 gap_us=314237	314237
816907312	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
816907742	ui	refreshWorkflowEditor	elapsed_ms=2	2000
816944983	worker	spike_loop_duration	iter=0	38298
816945716	ui	ui_fast_repeat_flush	pending_iters=1	517
816999338	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1330
817008094	ui	session_end	finish success=yes
820191712	ui	hotkey_trigger_dispatch	c5fdc932-f7a0-4de7-a75d-39126c373bd6	25
820192198	ui	session_begin	feature=/mute all mode=RepeatCount profile=LOL source=hotkey blocks=3
820193811	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
820194466	ui	refreshWorkflowEditor	elapsed_ms=1	1000
820199384	worker	spike_loop_gap	after_loop=0 gap_us=3254399	3254399
820290708	worker	spike_loop_duration	iter=0	91322
820295966	ui	ui_fast_repeat_flush	pending_iters=1	538
820296166	ui	session_end	finish success=yes
821620966	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
821623895	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3423
821625473	worker	spike_loop_gap	after_loop=0 gap_us=1334763	1334763
821626075	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
821626476	ui	refreshWorkflowEditor	elapsed_ms=1	1000
821663497	worker	spike_loop_duration	iter=0	38022
821664233	ui	ui_fast_repeat_flush	pending_iters=1	401
821739752	worker	loop_interval_sleep	requested_ms=75	76188
821739759	worker	spike_loop_gap	after_loop=0 gap_us=76261	76261
821773923	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1333
821777393	worker	spike_loop_duration	iter=1 loop=1	37631
821777730	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	285
821777857	ui	session_end	finish success=yes loop=1
821958926	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
821960784	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2424
821962475	worker	spike_loop_gap	after_loop=1 gap_us=185082	185082
821963231	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
821963614	ui	refreshWorkflowEditor	elapsed_ms=2	2000
822000464	worker	spike_loop_duration	iter=0	37985
822001054	ui	ui_fast_repeat_flush	pending_iters=1	419
822076235	worker	loop_interval_sleep	requested_ms=74	75682
822076241	worker	spike_loop_gap	after_loop=0 gap_us=75779	75779
822106469	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1157
822112894	worker	spike_loop_duration	iter=1 loop=1	36651
822113511	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	561
822114091	ui	session_end	finish success=yes loop=1
828278108	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
828280707	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3111
828282400	worker	spike_loop_gap	after_loop=1 gap_us=6169504	6169504
828282829	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
828283421	ui	refreshWorkflowEditor	elapsed_ms=2	2000
828325786	worker	spike_loop_duration	iter=0	43382
828326366	ui	ui_fast_repeat_flush	pending_iters=1	407
828379948	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1109
828389855	ui	session_end	finish success=yes
845802645	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
845805519	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3495
845807148	worker	spike_loop_gap	after_loop=0 gap_us=17481362	17481362
845808188	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
845808906	ui	refreshWorkflowEditor	elapsed_ms=2	2000
845846983	worker	spike_loop_duration	iter=0	39831
845847567	ui	ui_fast_repeat_flush	pending_iters=1	410
845899928	worker	loop_interval_sleep	requested_ms=52	52852
845899933	worker	spike_loop_gap	after_loop=0 gap_us=52951	52951
845914540	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1062
845944471	worker	spike_loop_duration	iter=1 loop=1	44535
845945134	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	379
845945139	ui	session_end	finish success=yes loop=1
846947919	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
846951165	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3793
846952914	worker	spike_loop_gap	after_loop=1 gap_us=1008443	1008443
846990370	worker	spike_loop_duration	iter=0	37454
846991692	ui	ui_fast_repeat_flush	pending_iters=1	940
847056324	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1192
847062165	ui	session_end	finish success=yes
847772216	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
847774796	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3116
847776334	worker	spike_loop_gap	after_loop=0 gap_us=785963	785963
847815713	worker	spike_loop_duration	iter=0	39375
847816321	ui	ui_fast_repeat_flush	pending_iters=1	407
847896572	worker	loop_interval_sleep	requested_ms=79	80742
847896579	worker	spike_loop_gap	after_loop=0 gap_us=80866	80866
847932750	worker	spike_loop_duration	iter=1 loop=1	36169
847936669	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	312
848014162	worker	loop_interval_sleep	requested_ms=80 loop=1	81356
848014168	worker	spike_loop_gap	after_loop=1 gap_us=81418 loop=1	81418
848043129	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=2	1133
848060957	worker	spike_loop_duration	iter=2 loop=2	46787
848061399	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	389
848061512	ui	session_end	finish success=yes loop=2
849101080	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
849103906	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3307
849105454	worker	spike_loop_gap	after_loop=2 gap_us=1044495	1044495
849146784	worker	spike_loop_duration	iter=0	41326
849147298	ui	ui_fast_repeat_flush	pending_iters=1	368
849218182	worker	loop_interval_sleep	requested_ms=70	71305
849218190	worker	spike_loop_gap	after_loop=0 gap_us=71405	71405
849254422	worker	spike_loop_duration	iter=1 loop=1	36229
849254867	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	385
849330756	worker	loop_interval_sleep	requested_ms=75 loop=1	76209
849330761	worker	spike_loop_gap	after_loop=1 gap_us=76340 loop=1	76340
849358830	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=2	986
849390147	worker	spike_loop_duration	iter=2 loop=2	59382
849390738	ui	ui_fast_repeat_flush	pending_iters=1 loop=2	326
849390743	ui	session_end	finish success=yes loop=2
854828676	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
854831762	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3593
854834292	worker	spike_loop_gap	after_loop=2 gap_us=5444144	5444144
854880169	worker	spike_loop_duration	iter=0	45873
854880859	ui	ui_fast_repeat_flush	pending_iters=1	419
854948842	worker	loop_interval_sleep	requested_ms=67	68554
854948849	worker	spike_loop_gap	after_loop=0 gap_us=68682	68682
854977173	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1207
854985940	worker	spike_loop_duration	iter=1 loop=1	37088
854986667	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	327
854986677	ui	session_end	finish success=yes loop=1
862020921	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
862024149	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3996
862025698	worker	spike_loop_gap	after_loop=1 gap_us=7039758	7039758
862026349	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
862026930	ui	refreshWorkflowEditor	elapsed_ms=1	1000
862064708	worker	spike_loop_duration	iter=0	39006
862065274	ui	ui_fast_repeat_flush	pending_iters=1	406
862145276	worker	loop_interval_sleep	requested_ms=79	80465
862145283	worker	spike_loop_gap	after_loop=0 gap_us=80575	80575
862178665	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1289
862181855	worker	spike_loop_duration	iter=1 loop=1	36570
862182348	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	283
862182352	ui	session_end	finish success=yes loop=1
871456986	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
871459666	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3297
871461399	worker	spike_loop_gap	after_loop=1 gap_us=9279542	9279542
871502560	worker	spike_loop_duration	iter=0	41159
871503131	ui	ui_fast_repeat_flush	pending_iters=1	411
871560596	worker	loop_interval_sleep	requested_ms=57	57945
871560603	worker	spike_loop_gap	after_loop=0 gap_us=58042	58042
871571221	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1182
871598413	worker	spike_loop_duration	iter=1 loop=1	37808
871598854	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	357
871599281	ui	session_end	finish success=yes loop=1
871700866	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
871703152	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2679
871704605	worker	spike_loop_gap	after_loop=1 gap_us=106189	106189
871710747	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
871711191	ui	refreshWorkflowEditor	elapsed_ms=1	1000
871744655	worker	spike_loop_duration	iter=0	40046
871745192	ui	ui_fast_repeat_flush	pending_iters=1	385
871754075	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1477
871756778	ui	session_end	finish success=yes
871840081	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
871841917	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2160
871843640	worker	spike_loop_gap	after_loop=0 gap_us=98986	98986
871885875	worker	spike_loop_duration	iter=0	42233
871886620	ui	ui_fast_repeat_flush	pending_iters=1	398
871938071	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1023
871946547	ui	session_end	finish success=yes
871988376	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
871993029	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4994
871995338	worker	spike_loop_gap	after_loop=0 gap_us=109456	109456
872038326	worker	spike_loop_duration	iter=0	42985
872040419	ui	ui_fast_repeat_flush	pending_iters=1	422
872094627	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1064
872102365	ui	session_end	finish success=yes
877592145	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
877594561	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3041
877596187	worker	spike_loop_gap	after_loop=0 gap_us=5557860	5557860
877596511	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
877597049	ui	refreshWorkflowEditor	elapsed_ms=2	2000
877635795	worker	spike_loop_duration	iter=0	39606
877636500	ui	ui_fast_repeat_flush	pending_iters=1	411
877691058	worker	loop_interval_sleep	requested_ms=54	55219
877691065	worker	spike_loop_gap	after_loop=0 gap_us=55269	55269
877722674	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	2219
877736733	worker	spike_loop_duration	iter=1 loop=1	45666
877737372	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	317
877737376	ui	session_end	finish success=yes loop=1
881421582	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
881423901	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2784
881425634	worker	spike_loop_gap	after_loop=1 gap_us=3688899	3688899
881426105	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
881426705	ui	refreshWorkflowEditor	elapsed_ms=2	2000
881464153	worker	spike_loop_duration	iter=0	38516
881464770	ui	ui_fast_repeat_flush	pending_iters=1	430
881528414	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1133
881541544	ui	session_end	finish success=yes
881600977	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
881603249	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2762
881604850	worker	spike_loop_gap	after_loop=0 gap_us=140696	140696
881641537	worker	spike_loop_duration	iter=0	36684
881642901	ui	ui_fast_repeat_flush	pending_iters=1	460
881701955	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1119
881709729	ui	session_end	finish success=yes
881757313	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
881760601	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3756
881762230	worker	spike_loop_gap	after_loop=0 gap_us=120692	120692
881802597	worker	spike_loop_duration	iter=0	40364
881803201	ui	ui_fast_repeat_flush	pending_iters=1	436
881881183	worker	loop_interval_sleep	requested_ms=77	78493
881881191	worker	spike_loop_gap	after_loop=0 gap_us=78594	78594
881887397	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	886
881917510	worker	spike_loop_duration	iter=1 loop=1	36315
881918096	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	331
881918100	ui	session_end	finish success=yes loop=1
883537306	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
883545123	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	8321
883546783	worker	spike_loop_gap	after_loop=1 gap_us=1629272	1629272
883548443	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
883548934	ui	refreshWorkflowEditor	elapsed_ms=2	2000
883586828	worker	spike_loop_duration	iter=0	40044
883588532	ui	ui_fast_repeat_flush	pending_iters=1	756
883650410	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	981
883658984	ui	session_end	finish success=yes
888077386	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
888080273	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3477
888081964	worker	spike_loop_gap	after_loop=0 gap_us=4495134	4495134
888082855	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
888083649	ui	refreshWorkflowEditor	elapsed_ms=2	2000
888121390	worker	spike_loop_duration	iter=0	39421
888121983	ui	ui_fast_repeat_flush	pending_iters=1	427
888191073	worker	loop_interval_sleep	requested_ms=68	69605
888191077	worker	spike_loop_gap	after_loop=0 gap_us=69689	69689
888229341	worker	spike_loop_duration	iter=1 loop=1	38260
888229790	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	387
888248762	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1178
888251397	ui	session_end	finish success=yes loop=1
891351958	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
891354516	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3074
891356280	worker	spike_loop_gap	after_loop=1 gap_us=3126937	3126937
891356961	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
891357544	ui	refreshWorkflowEditor	elapsed_ms=2	2000
891394740	worker	spike_loop_duration	iter=0	38457
891395669	ui	ui_fast_repeat_flush	pending_iters=1	428
891468954	worker	loop_interval_sleep	requested_ms=72	73965
891468961	worker	spike_loop_gap	after_loop=0 gap_us=74222	74222
891487362	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	908
891505224	worker	spike_loop_duration	iter=1 loop=1	36261
891505782	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	456
891505817	ui	session_end	finish success=yes loop=1
898305552	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
898308649	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3661
898310204	worker	spike_loop_gap	after_loop=1 gap_us=6804979	6804979
898310710	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
898311365	ui	refreshWorkflowEditor	elapsed_ms=2	2000
898351111	worker	spike_loop_duration	iter=0	40904
898351648	ui	ui_fast_repeat_flush	pending_iters=1	392
898408496	worker	loop_interval_sleep	requested_ms=56	57292
898408502	worker	spike_loop_gap	after_loop=0 gap_us=57390	57390
898440811	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1245
898444688	worker	spike_loop_duration	iter=1 loop=1	36184
898445659	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	903
898445902	ui	session_end	finish success=yes loop=1
898668420	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
898670987	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3127
898672564	worker	spike_loop_gap	after_loop=1 gap_us=227875	227875
898709527	worker	spike_loop_duration	iter=0	36958
898710222	ui	ui_fast_repeat_flush	pending_iters=1	420
898767661	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	936
898772432	ui	session_end	finish success=yes
898846323	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
898849616	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3766
898851265	worker	spike_loop_gap	after_loop=0 gap_us=141738	141738
898891241	worker	spike_loop_duration	iter=0	39974
898892238	ui	ui_fast_repeat_flush	pending_iters=1	543
898945615	worker	loop_interval_sleep	requested_ms=53	54270
898945621	worker	spike_loop_gap	after_loop=0 gap_us=54380	54380
898958720	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1248
898987169	worker	spike_loop_duration	iter=1 loop=1	41544
898988110	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	570
898988116	ui	session_end	finish success=yes loop=1
899029627	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
899033197	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4142
899035242	worker	spike_loop_gap	after_loop=1 gap_us=48071	48071
899095586	worker	spike_loop_duration	iter=0	60341
899100872	ui	ui_fast_repeat_flush	pending_iters=1	390
899148170	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4438
899148768	ui	session_end	finish success=yes
900323781	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
900329031	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	6094
900330906	worker	spike_loop_gap	after_loop=0 gap_us=1235317	1235317
900336308	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
900336919	ui	refreshWorkflowEditor	elapsed_ms=1	1000
900371300	worker	spike_loop_duration	iter=0	40390
900376095	ui	ui_fast_repeat_flush	pending_iters=1	845
900437865	worker	loop_interval_sleep	requested_ms=65	66419
900437872	worker	spike_loop_gap	after_loop=0 gap_us=66572	66572
900740519	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	297815
900774389	worker	spike_synthetic_key	dur_us=336486 loop=1	336486
900774405	worker	spike_loop_duration	iter=1 loop=1	336532
900775100	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	517
900775129	ui	session_end	finish success=yes loop=1
907666083	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
907668430	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2803
907670504	worker	spike_loop_gap	after_loop=1 gap_us=6896098	6896098
907711792	worker	spike_loop_duration	iter=0	41282
907712440	ui	ui_fast_repeat_flush	pending_iters=1	440
907780459	worker	loop_interval_sleep	requested_ms=67	68569
907780465	worker	spike_loop_gap	after_loop=0 gap_us=68676	68676
907786868	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1210
907816690	worker	spike_loop_duration	iter=1 loop=1	36221
907817214	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	456
907817323	ui	session_end	finish success=yes loop=1
908907947	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
908910444	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3146
908912137	worker	spike_loop_gap	after_loop=1 gap_us=1095448	1095448
908912512	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
908912959	ui	refreshWorkflowEditor	elapsed_ms=2	2000
908953685	worker	spike_loop_duration	iter=0	41543
908954662	ui	ui_fast_repeat_flush	pending_iters=1	719
909017423	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1902
909025341	ui	session_end	finish success=yes
909430132	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
909432683	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3256
909434278	worker	spike_loop_gap	after_loop=0 gap_us=480593	480593
909434684	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
909435101	ui	refreshWorkflowEditor	elapsed_ms=1	1000
909475954	worker	spike_loop_duration	iter=0	41672
909476514	ui	ui_fast_repeat_flush	pending_iters=1	385
909533224	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1103
909540193	ui	session_end	finish success=yes
911111658	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
911114334	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3185
911116729	worker	spike_loop_gap	after_loop=0 gap_us=1640775	1640775
911156301	worker	spike_loop_duration	iter=0	39570
911157372	ui	ui_fast_repeat_flush	pending_iters=1	727
911223047	worker	loop_interval_sleep	requested_ms=65	66674
911223055	worker	spike_loop_gap	after_loop=0 gap_us=66753	66753
911241797	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	978
911259299	worker	spike_loop_duration	iter=1 loop=1	36242
911259736	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	377
911259905	ui	session_end	finish success=yes loop=1
912638802	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
912641018	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2933
912642988	worker	spike_loop_gap	after_loop=1 gap_us=1383687	1383687
912643425	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
912643842	ui	refreshWorkflowEditor	elapsed_ms=2	2000
912680811	worker	spike_loop_duration	iter=0	37820
912681508	ui	ui_fast_repeat_flush	pending_iters=1	428
912733101	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1116
912744318	ui	session_end	finish success=yes
912808008	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
912810519	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2995
912812197	worker	spike_loop_gap	after_loop=0 gap_us=131386	131386
912851110	worker	spike_loop_duration	iter=0	38910
912851679	ui	ui_fast_repeat_flush	pending_iters=1	403
912907859	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1275
912911763	ui	session_end	finish success=yes
912958352	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
912961145	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3256
912963072	worker	spike_loop_gap	after_loop=0 gap_us=111961	111961
913002230	worker	spike_loop_duration	iter=0	39155
913002879	ui	ui_fast_repeat_flush	pending_iters=1	469
913080126	worker	loop_interval_sleep	requested_ms=76	77796
913080134	worker	spike_loop_gap	after_loop=0 gap_us=77902	77902
913082852	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1384
913117437	worker	spike_loop_duration	iter=1 loop=1	37299
913118318	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	534
913118324	ui	session_end	finish success=yes loop=1
913728744	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
913731385	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3145
913733408	worker	spike_loop_gap	after_loop=1 gap_us=615971	615971
913733822	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
913734198	ui	refreshWorkflowEditor	elapsed_ms=2	2000
913770914	worker	spike_loop_duration	iter=0	37501
913772119	ui	ui_fast_repeat_flush	pending_iters=1	608
913845708	worker	loop_interval_sleep	requested_ms=73	74675
913845714	worker	spike_loop_gap	after_loop=0 gap_us=74801	74801
913846910	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1431
913885662	worker	spike_loop_duration	iter=1 loop=1	39945
913886516	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	552
913886521	ui	session_end	finish success=yes loop=1
915063681	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
915068205	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	5311
915070874	worker	spike_loop_gap	after_loop=1 gap_us=1185210	1185210
915076662	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
915077448	ui	refreshWorkflowEditor	elapsed_ms=2	2000
915109761	worker	spike_loop_duration	iter=0	38882
915110330	ui	ui_fast_repeat_flush	pending_iters=1	406
915191636	worker	loop_interval_sleep	requested_ms=80	81785
915191641	worker	spike_loop_gap	after_loop=0 gap_us=81882	81882
915195239	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	869
915228753	worker	spike_loop_duration	iter=1 loop=1	37109
915229166	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	356
915229288	ui	session_end	finish success=yes loop=1
916963380	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
916965903	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3074
916967446	worker	spike_loop_gap	after_loop=1 gap_us=1738692	1738692
917006696	worker	spike_loop_duration	iter=0	39248
917007240	ui	ui_fast_repeat_flush	pending_iters=1	381
917070272	worker	loop_interval_sleep	requested_ms=62	63480
917070277	worker	spike_loop_gap	after_loop=0 gap_us=63581	63581
917082551	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1444
917107976	worker	spike_loop_duration	iter=1 loop=1	37695
917108354	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	322
917108471	ui	session_end	finish success=yes loop=1
917374442	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
917376880	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3072
917378854	worker	spike_loop_gap	after_loop=1 gap_us=270878	270878
917379603	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
917380079	ui	refreshWorkflowEditor	elapsed_ms=2	2000
917417431	worker	spike_loop_duration	iter=0	38572
917418008	ui	ui_fast_repeat_flush	pending_iters=1	403
917487118	worker	loop_interval_sleep	requested_ms=68	69589
917487124	worker	spike_loop_gap	after_loop=0 gap_us=69695	69695
917509241	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1127
917543648	worker	spike_loop_duration	iter=1 loop=1	56522
917544764	ui	session_end	reason=preempted_by_new_session loop=1
917544816	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
917547623	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3819
917586370	worker	spike_loop_duration	iter=0	37030
917587429	ui	ui_fast_repeat_flush	pending_iters=1	423
917642530	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1105
917647688	ui	session_end	finish success=yes
917694590	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
917697213	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3086
917698948	worker	spike_loop_gap	after_loop=0 gap_us=112577	112577
917738997	worker	spike_loop_duration	iter=0	40045
917739642	ui	ui_fast_repeat_flush	pending_iters=1	429
917798227	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	885
917804287	ui	session_end	finish success=yes
917846496	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
917850338	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4353
917855094	worker	spike_loop_gap	after_loop=0 gap_us=116092	116092
917892083	worker	spike_loop_duration	iter=0	36983
917893444	ui	ui_fast_repeat_flush	pending_iters=1	706
917954875	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1176
917964096	ui	session_end	finish success=yes
920536247	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
920538270	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2392
920539887	worker	spike_loop_gap	after_loop=0 gap_us=2647794	2647794
920577766	worker	spike_loop_duration	iter=0	37875
920578339	ui	ui_fast_repeat_flush	pending_iters=1	401
920630114	worker	loop_interval_sleep	requested_ms=51	52241
920630121	worker	spike_loop_gap	after_loop=0 gap_us=52356	52356
920642824	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	983
920673989	worker	spike_loop_duration	iter=1 loop=1	43865
920674410	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	354
920674443	ui	session_end	finish success=yes loop=1
920712844	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
920714724	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2186
920716241	worker	spike_loop_gap	after_loop=1 gap_us=42251	42251
920767386	worker	spike_loop_duration	iter=0	51143
920773810	ui	ui_fast_repeat_flush	pending_iters=1	463
920778053	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1046
920782250	ui	session_end	finish success=yes
920879140	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
920882293	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3507
920884105	worker	spike_loop_gap	after_loop=0 gap_us=116715	116715
920923610	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1112
920923939	worker	spike_loop_duration	iter=0	39831
920924953	ui	ui_fast_repeat_flush	pending_iters=1	348
920924956	ui	session_end	finish success=yes
921012405	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
921014528	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2498
921016367	worker	spike_loop_gap	after_loop=0 gap_us=92423	92423
921058059	worker	spike_loop_duration	iter=0	41688
921059050	ui	ui_fast_repeat_flush	pending_iters=1	418
921082064	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1958
921128370	ui	session_end	finish success=yes
921168578	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
921170316	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2073
921172032	worker	spike_loop_gap	after_loop=0 gap_us=113970	113970
921209864	worker	spike_loop_duration	iter=0	37827
921210478	ui	ui_fast_repeat_flush	pending_iters=1	413
921220028	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1508
921263427	ui	session_end	finish success=yes
921305389	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
921307477	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2487
921309344	worker	spike_loop_gap	after_loop=0 gap_us=99480	99480
921347091	worker	spike_loop_duration	iter=0	37745
921347906	ui	ui_fast_repeat_flush	pending_iters=1	612
921407446	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1362
921412401	ui	session_end	finish success=yes
921450150	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
921452482	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2668
921454230	worker	spike_loop_gap	after_loop=0 gap_us=107135	107135
921492008	worker	spike_loop_duration	iter=0	37775
921492641	ui	ui_fast_repeat_flush	pending_iters=1	393
921547343	worker	loop_interval_sleep	requested_ms=54	55173
921547351	worker	spike_loop_gap	after_loop=0 gap_us=55343	55343
921564846	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	882
921595329	worker	spike_loop_duration	iter=1 loop=1	47975
921595791	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	399
921595839	ui	session_end	finish success=yes loop=1
922257963	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
922261434	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3963
922263187	worker	spike_loop_gap	after_loop=1 gap_us=667857	667857
922299528	worker	spike_loop_duration	iter=0	36337
922301748	ui	ui_fast_repeat_flush	pending_iters=1	1349
922366297	worker	loop_interval_sleep	requested_ms=65	66700
922366302	worker	spike_loop_gap	after_loop=0 gap_us=66775	66775
922400201	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1408
922403319	worker	spike_loop_duration	iter=1 loop=1	37014
922403757	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	269
922403761	ui	session_end	finish success=yes loop=1
924720401	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
924723225	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3334
924724721	worker	spike_loop_gap	after_loop=1 gap_us=2321401	2321401
924764053	worker	spike_loop_duration	iter=0	39329
924764859	ui	ui_fast_repeat_flush	pending_iters=1	438
924832304	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1064
924840268	ui	session_end	finish success=yes
925027422	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
925029715	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2799
925031536	worker	spike_loop_gap	after_loop=0 gap_us=267481	267481
925031945	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
925032348	ui	refreshWorkflowEditor	elapsed_ms=2	2000
925069330	worker	spike_loop_duration	iter=0	37790
925070009	ui	ui_fast_repeat_flush	pending_iters=1	462
925133876	worker	loop_interval_sleep	requested_ms=63	64441
925133881	worker	spike_loop_gap	after_loop=0 gap_us=64552	64552
925437043	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	299514
925470810	worker	spike_synthetic_key	dur_us=336910 loop=1	336910
925470826	worker	spike_loop_duration	iter=1 loop=1	336943
925471599	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	687
925471633	ui	session_end	finish success=yes loop=1
926320669	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
926323503	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3311
926325007	worker	spike_loop_gap	after_loop=1 gap_us=854180	854180
926366549	worker	spike_loop_duration	iter=0	41539
926367147	ui	ui_fast_repeat_flush	pending_iters=1	383
926426033	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	1174
926431494	ui	session_end	finish success=yes
926861222	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
926863925	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3227
926865538	worker	spike_loop_gap	after_loop=0 gap_us=498989	498989
926905201	worker	spike_loop_duration	iter=0	39658
926905867	ui	ui_fast_repeat_flush	pending_iters=1	490
926974379	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	916
926978460	ui	session_end	finish success=yes
927446821	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
927449826	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3794
927451438	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
927451770	worker	spike_loop_gap	after_loop=0 gap_us=546570	546570
927452320	ui	refreshWorkflowEditor	elapsed_ms=2	2000
927490932	worker	spike_loop_duration	iter=0	39158
927491451	ui	ui_fast_repeat_flush	pending_iters=1	380
927551811	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1476
927556070	ui	session_end	finish success=yes
931502994	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
931505779	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3501
931507596	worker	spike_loop_gap	after_loop=0 gap_us=4016664	4016664
931507807	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
931508449	ui	refreshWorkflowEditor	elapsed_ms=2	2000
931546168	worker	spike_loop_duration	iter=0	38568
931546913	ui	ui_fast_repeat_flush	pending_iters=1	492
931604445	worker	loop_interval_sleep	requested_ms=57	58157
931604453	worker	spike_loop_gap	after_loop=0 gap_us=58286	58286
931612231	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1068
931643263	worker	spike_loop_duration	iter=1 loop=1	38805
931643844	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	506
931643907	ui	session_end	finish success=yes loop=1
931681850	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
931683747	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2398
931685334	worker	spike_loop_gap	after_loop=1 gap_us=42072	42072
931691913	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
931692284	ui	refreshWorkflowEditor	elapsed_ms=1	1000
931737925	worker	spike_loop_duration	iter=0	52585
931742613	ui	session_end	finish success=yes
931775142	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	32977
931788747	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
931790781	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2502
931792634	worker	spike_loop_gap	after_loop=0 gap_us=54707	54707
931832867	worker	spike_loop_duration	iter=0	40231
931836941	ui	ui_fast_repeat_flush	pending_iters=1	436
931891216	worker	loop_interval_sleep	requested_ms=57	58279
931891225	worker	spike_loop_gap	after_loop=0 gap_us=58357	58357
931902490	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1102
931931537	worker	spike_loop_duration	iter=1 loop=1	40309
931932024	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	393
931932097	ui	session_end	finish success=yes loop=1
932612541	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
932615215	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3248
932616932	worker	spike_loop_gap	after_loop=1 gap_us=685395	685395
932617570	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
932617982	ui	refreshWorkflowEditor	elapsed_ms=2	2000
932655105	worker	spike_loop_duration	iter=0	38168
932656691	ui	ui_fast_repeat_flush	pending_iters=1	446
932711109	worker	loop_interval_sleep	requested_ms=54	55099
932711115	worker	spike_loop_gap	after_loop=0 gap_us=56012	56012
932724811	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	883
932755127	worker	spike_loop_duration	iter=1 loop=1	44009
932755948	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	547
932755953	ui	session_end	finish success=yes loop=1
932875686	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
932879794	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	4624
932881435	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
932881620	worker	spike_loop_gap	after_loop=1 gap_us=126493	126493
932881946	ui	refreshWorkflowEditor	elapsed_ms=1	1000
932918910	worker	spike_loop_duration	iter=0	37285
932919791	ui	ui_fast_repeat_flush	pending_iters=1	601
932982456	worker	loop_interval_sleep	requested_ms=62	63448
932982462	worker	spike_loop_gap	after_loop=0 gap_us=63554	63554
932991488	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1125
933020748	worker	spike_loop_duration	iter=1 loop=1	38284
933022070	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	873
933022075	ui	session_end	finish success=yes loop=1
933065716	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
933069441	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	4582
933071183	worker	spike_loop_gap	after_loop=1 gap_us=50431	50431
933130231	worker	spike_loop_duration	iter=0	59046
933134737	ui	ui_fast_repeat_flush	pending_iters=1	377
933145356	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	887
933150753	ui	session_end	finish success=yes
933873105	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
933875611	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2976
933877192	worker	spike_loop_gap	after_loop=0 gap_us=746960	746960
933877586	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
933878023	ui	refreshWorkflowEditor	elapsed_ms=1	1000
933915844	worker	spike_loop_duration	iter=0	38648
933916789	ui	ui_fast_repeat_flush	pending_iters=1	408
933974439	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1002
933979424	ui	session_end	finish success=yes
934170424	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
934173263	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3361
934174981	worker	spike_loop_gap	after_loop=0 gap_us=259138	259138
934175183	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
934175663	ui	refreshWorkflowEditor	elapsed_ms=1	1000
934212666	worker	spike_loop_duration	iter=0	37681
934213282	ui	ui_fast_repeat_flush	pending_iters=1	412
934235811	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	939
934282676	ui	session_end	finish success=yes
934332642	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
934336280	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	4732
934338077	worker	spike_loop_gap	after_loop=0 gap_us=125409	125409
934378243	worker	spike_loop_duration	iter=0	40164
934378993	ui	ui_fast_repeat_flush	pending_iters=1	429
934438660	worker	loop_interval_sleep	requested_ms=59	60316
934438666	worker	spike_loop_gap	after_loop=0 gap_us=60422	60422
934445853	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	948
934476546	worker	spike_loop_duration	iter=1 loop=1	37877
934477247	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	599
934477383	ui	session_end	finish success=yes loop=1
1010168129	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1010170975	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3335
1010172758	worker	spike_loop_gap	after_loop=1 gap_us=75696213	75696213
1010173351	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1010173936	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1010211491	worker	spike_loop_duration	iter=0	38729
1010212222	ui	ui_fast_repeat_flush	pending_iters=1	404
1010291305	worker	loop_interval_sleep	requested_ms=78	79744
1010291312	worker	spike_loop_gap	after_loop=0 gap_us=79822	79822
1010293596	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1119
1010327728	worker	spike_loop_duration	iter=1 loop=1	36414
1010328252	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	318
1010328257	ui	session_end	finish success=yes loop=1
1010501329	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1010504603	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3721
1010506200	worker	spike_loop_gap	after_loop=1 gap_us=178472	178472
1010546011	worker	spike_loop_duration	iter=0	39806
1010546717	ui	ui_fast_repeat_flush	pending_iters=1	441
1010599933	worker	loop_interval_sleep	requested_ms=53	53829
1010599940	worker	spike_loop_gap	after_loop=0 gap_us=53930	53930
1010616083	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1129
1010646536	worker	spike_loop_duration	iter=1 loop=1	46592
1010647614	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	349
1010647619	ui	session_end	finish success=yes loop=1
1016995729	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1016998713	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3761
1017000208	worker	spike_loop_gap	after_loop=1 gap_us=6353673	6353673
1017000648	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1017001239	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1017038868	worker	spike_loop_duration	iter=0	38656
1017039590	ui	ui_fast_repeat_flush	pending_iters=1	403
1017108359	worker	loop_interval_sleep	requested_ms=68	69431
1017108364	worker	spike_loop_gap	after_loop=0 gap_us=69498	69498
1017128336	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1579
1017145172	worker	spike_loop_duration	iter=1 loop=1	36805
1017145623	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	389
1017145665	ui	session_end	finish success=yes loop=1
1017621414	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1017623897	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2996
1017625631	worker	spike_loop_gap	after_loop=1 gap_us=480459	480459
1017626185	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1017626697	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1017663335	worker	spike_loop_duration	iter=0	37699
1017664194	ui	ui_fast_repeat_flush	pending_iters=1	564
1017743886	worker	loop_interval_sleep	requested_ms=79	80452
1017743893	worker	spike_loop_gap	after_loop=0 gap_us=80558	80558
1017768038	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1158
1017780697	worker	spike_loop_duration	iter=1 loop=1	36802
1017781096	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	324
1017781158	ui	session_end	finish success=yes loop=1
1023274652	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1023277154	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2998
1023278730	worker	spike_loop_gap	after_loop=1 gap_us=5498033	5498033
1023279096	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1023279716	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1023324305	worker	spike_loop_duration	iter=0	45570
1023324884	ui	ui_fast_repeat_flush	pending_iters=1	415
1023384956	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	930
1023394605	ui	session_end	finish success=yes
1023438259	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1023440128	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2191
1023441740	worker	spike_loop_gap	after_loop=0 gap_us=117436	117436
1023479208	worker	spike_loop_duration	iter=0	37465
1023479951	ui	ui_fast_repeat_flush	pending_iters=1	494
1023548022	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1185
1023551583	ui	session_end	finish success=yes
1023621251	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1023623707	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3069
1023625170	worker	spike_loop_gap	after_loop=0 gap_us=145961	145961
1023664781	worker	spike_loop_duration	iter=0	39607
1023665389	ui	ui_fast_repeat_flush	pending_iters=1	436
1023725103	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	977
1023729652	ui	session_end	finish success=yes
1024292572	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1024295448	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3336
1024297109	worker	spike_loop_gap	after_loop=0 gap_us=632328	632328
1024337554	worker	spike_loop_duration	iter=0	40444
1024338541	ui	ui_fast_repeat_flush	pending_iters=1	649
1024396000	worker	loop_interval_sleep	requested_ms=57	58389
1024396006	worker	spike_loop_gap	after_loop=0 gap_us=58451	58451
1024406197	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1322
1024435487	worker	spike_loop_duration	iter=1 loop=1	39478
1024436028	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	346
1024436032	ui	session_end	finish success=yes loop=1
1024473552	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1024476098	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3231
1024477927	worker	spike_loop_gap	after_loop=1 gap_us=42439	42439
1024536530	worker	spike_loop_duration	iter=0	58601
1024541484	ui	ui_fast_repeat_flush	pending_iters=1	443
1024581087	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2573
1024599130	ui	session_end	finish success=yes
1024683701	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1024685479	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2095
1024687045	worker	spike_loop_gap	after_loop=0 gap_us=150513	150513
1024725298	worker	spike_loop_duration	iter=0	38248
1024725986	ui	ui_fast_repeat_flush	pending_iters=1	420
1024801413	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1409
1024811029	ui	session_end	finish success=yes
1025280909	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1025283488	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3117
1025285236	worker	spike_loop_gap	after_loop=0 gap_us=559937	559937
1025325194	worker	spike_loop_duration	iter=0	39953
1025326043	ui	ui_fast_repeat_flush	pending_iters=1	683
1025389581	worker	loop_interval_sleep	requested_ms=63	64288
1025389588	worker	spike_loop_gap	after_loop=0 gap_us=64395	64395
1025399777	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1359
1025427993	worker	spike_loop_duration	iter=1 loop=1	38402
1025428612	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	542
1025430036	ui	session_end	finish success=yes loop=1
1025471108	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1025473223	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2439
1025474851	worker	spike_loop_gap	after_loop=1 gap_us=46856	46856
1025529908	worker	spike_loop_duration	iter=0	55055
1025531354	ui	ui_fast_repeat_flush	pending_iters=1	338
1025539956	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	906
1025542645	ui	session_end	finish success=yes
1025629136	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1025631581	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2801
1025633524	worker	spike_loop_gap	after_loop=0 gap_us=103613	103613
1025672282	worker	spike_loop_duration	iter=0	38754
1025673064	ui	ui_fast_repeat_flush	pending_iters=1	421
1025739477	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2179
1025744585	ui	session_end	finish success=yes
1028748431	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1028751119	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3265
1028752914	worker	spike_loop_gap	after_loop=0 gap_us=3080632	3080632
1028753488	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1028753915	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1028790688	worker	spike_loop_duration	iter=0	37770
1028791657	ui	ui_fast_repeat_flush	pending_iters=1	771
1028797283	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1294
1028800905	ui	session_end	finish success=yes
1033102662	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1033105231	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3089
1033107280	worker	spike_loop_gap	after_loop=0 gap_us=4316591	4316591
1033148406	worker	spike_loop_duration	iter=0	41122
1033149551	ui	ui_fast_repeat_flush	pending_iters=1	711
1033152960	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1278
1033158948	ui	session_end	finish success=yes
1037019385	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1037022271	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3782
1037023809	worker	spike_loop_gap	after_loop=0 gap_us=3875403	3875403
1037024546	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1037025110	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1037067036	worker	spike_loop_duration	iter=0	43223
1037067909	ui	ui_fast_repeat_flush	pending_iters=1	438
1037126233	worker	loop_interval_sleep	requested_ms=58	59100
1037126238	worker	spike_loop_gap	after_loop=0 gap_us=59204	59204
1037143094	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1002
1037172182	worker	spike_loop_duration	iter=1 loop=1	45941
1037172998	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	729
1037173121	ui	session_end	finish success=yes loop=1
1041644242	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1041647213	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3445
1041648961	worker	spike_loop_gap	after_loop=1 gap_us=4476779	4476779
1041649635	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1041650250	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1041688052	worker	spike_loop_duration	iter=0	39085
1041688699	ui	ui_fast_repeat_flush	pending_iters=1	432
1041755830	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1009
1041760816	ui	session_end	finish success=yes
1041820871	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1041823850	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3432
1041825580	worker	spike_loop_gap	after_loop=0 gap_us=137530	137530
1041826161	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1041826593	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1041863799	worker	spike_loop_duration	iter=0	38214
1041864555	ui	ui_fast_repeat_flush	pending_iters=1	441
1041930594	worker	loop_interval_sleep	requested_ms=65	66717
1041930604	worker	spike_loop_gap	after_loop=0 gap_us=66806	66806
1041946286	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1650
1041972477	worker	spike_loop_duration	iter=1 loop=1	41871
1041973141	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	593
1041973171	ui	session_end	finish success=yes loop=1
1042016088	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1042017838	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2060
1042019674	worker	spike_loop_gap	after_loop=1 gap_us=47195	47195
1042080460	worker	spike_loop_duration	iter=0	60784
1042081768	ui	session_end	finish success=yes
1042113865	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	32658
1042128297	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1042132717	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4944
1042134583	worker	spike_loop_gap	after_loop=0 gap_us=54119	54119
1042179272	worker	spike_loop_duration	iter=0	44683
1042180495	ui	ui_fast_repeat_flush	pending_iters=1	659
1042245182	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1511
1042251171	ui	session_end	finish success=yes
1045738383	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1045741168	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3315
1045743431	worker	spike_loop_gap	after_loop=0 gap_us=3564160	3564160
1045743825	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1045744413	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1045781486	worker	spike_loop_duration	iter=0	38051
1045782063	ui	ui_fast_repeat_flush	pending_iters=1	406
1045849045	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1077
1045853724	ui	session_end	finish success=yes
1049228696	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1049231116	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2906
1049232877	worker	spike_loop_gap	after_loop=0 gap_us=3451390	3451390
1049270086	worker	spike_loop_duration	iter=0	37206
1049270783	ui	ui_fast_repeat_flush	pending_iters=1	419
1049329607	worker	loop_interval_sleep	requested_ms=58	59404
1049329614	worker	spike_loop_gap	after_loop=0 gap_us=59527	59527
1049344147	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1188
1049373824	worker	spike_loop_duration	iter=1 loop=1	44208
1049374507	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	557
1049374649	ui	session_end	finish success=yes loop=1
1049416083	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1049418334	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2574
1049420261	worker	spike_loop_gap	after_loop=1 gap_us=46435	46435
1049423515	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1049423912	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1049480617	worker	spike_loop_duration	iter=0	60354
1049489254	ui	ui_fast_repeat_flush	pending_iters=1	397
1049490210	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	953
1049496144	ui	session_end	finish success=yes
1049610439	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1049613854	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3895
1049615514	worker	spike_loop_gap	after_loop=0 gap_us=134894	134894
1049647776	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1275
1049655568	worker	spike_loop_duration	iter=0	40049
1049656382	ui	ui_fast_repeat_flush	pending_iters=1	626
1049656385	ui	session_end	finish success=yes
1049743170	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1049745017	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2168
1049747083	worker	spike_loop_gap	after_loop=0 gap_us=91513	91513
1049787226	worker	spike_loop_duration	iter=0	40139
1049787798	ui	ui_fast_repeat_flush	pending_iters=1	323
1049849612	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	927
1049858857	ui	session_end	finish success=yes
1049896007	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1049898425	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2780
1049900227	worker	spike_loop_gap	after_loop=0 gap_us=112999	112999
1049940023	worker	spike_loop_duration	iter=0	39793
1049940787	ui	ui_fast_repeat_flush	pending_iters=1	470
1049951920	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2077
1050000934	ui	session_end	finish success=yes
1053331276	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1053334892	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	4145
1053336796	worker	spike_loop_gap	after_loop=0 gap_us=3396770	3396770
1053341228	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1053341993	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1053380223	worker	spike_loop_duration	iter=0	43422
1053381197	ui	ui_fast_repeat_flush	pending_iters=1	755
1053443712	worker	loop_interval_sleep	requested_ms=62	63378
1053443717	worker	spike_loop_gap	after_loop=0 gap_us=63496	63496
1053444569	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1033
1053485863	worker	spike_loop_duration	iter=1 loop=1	42143
1053486486	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	332
1053486490	ui	session_end	finish success=yes loop=1
1053521644	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1053523500	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3008
1053525031	worker	spike_loop_gap	after_loop=1 gap_us=39168	39168
1053582382	worker	spike_loop_duration	iter=0	57348
1053587837	ui	ui_fast_repeat_flush	pending_iters=1	376
1053605245	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1122
1053613452	ui	session_end	finish success=yes
1053697585	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1053699459	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2207
1053701593	worker	spike_loop_gap	after_loop=0 gap_us=119204	119204
1053743433	worker	spike_loop_duration	iter=0	41835
1053744133	ui	ui_fast_repeat_flush	pending_iters=1	422
1053796105	worker	loop_interval_sleep	requested_ms=51	52555
1053796111	worker	spike_loop_gap	after_loop=0 gap_us=52680	52680
1053825525	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	962
1053855495	worker	spike_loop_duration	iter=1 loop=1	59381
1053855911	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	347
1053855942	ui	session_end	finish success=yes loop=1
1053899635	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1053902180	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3519
1053903897	worker	spike_loop_gap	after_loop=1 gap_us=48400	48400
1053956417	worker	spike_loop_duration	iter=0	52518
1053962426	ui	session_end	finish success=yes
1053995252	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	33465
1054031314	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1054034413	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3635
1054036860	worker	spike_loop_gap	after_loop=0 gap_us=80440	80440
1054077256	worker	spike_loop_duration	iter=0	40391
1054077875	ui	ui_fast_repeat_flush	pending_iters=1	425
1054131402	worker	loop_interval_sleep	requested_ms=53	54009
1054131412	worker	spike_loop_gap	after_loop=0 gap_us=54157	54157
1054149559	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1201
1054177818	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	81
1054179267	worker	spike_loop_duration	iter=1 loop=1	47854
1054179852	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	474
1054179882	ui	session_end	finish success=yes loop=1
1054282494	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	10
1054340459	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1054343521	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3617
1054345436	worker	spike_loop_gap	after_loop=1 gap_us=166167	166167
1054386207	worker	spike_loop_duration	iter=0	40769
1054386959	ui	ui_fast_repeat_flush	pending_iters=1	419
1054451842	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1360
1054459242	ui	session_end	finish success=yes
1054508156	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1054510548	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2905
1054512291	worker	spike_loop_gap	after_loop=0 gap_us=126082	126082
1054552539	worker	spike_loop_duration	iter=0	40244
1054553209	ui	ui_fast_repeat_flush	pending_iters=1	398
1054605992	worker	loop_interval_sleep	requested_ms=52	53354
1054605998	worker	spike_loop_gap	after_loop=0 gap_us=53460	53460
1054620517	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1535
1054649512	worker	spike_loop_duration	iter=1 loop=1	43511
1054649984	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	365
1054650019	ui	session_end	finish success=yes loop=1
1054692505	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1054694414	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2209
1054696168	worker	spike_loop_gap	after_loop=1 gap_us=46654	46654
1054751338	worker	spike_loop_duration	iter=0	55167
1054756441	ui	ui_fast_repeat_flush	pending_iters=1	381
1054804729	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1530
1054812996	ui	session_end	finish success=yes
1057675406	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1057679144	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4286
1057681163	worker	spike_loop_gap	after_loop=0 gap_us=2929824	2929824
1057681943	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1057682388	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1057723290	worker	spike_loop_duration	iter=0	42122
1057723954	ui	ui_fast_repeat_flush	pending_iters=1	422
1057782986	worker	loop_interval_sleep	requested_ms=58	59580
1057782994	worker	spike_loop_gap	after_loop=0 gap_us=59705	59705
1057791407	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	934
1057821625	worker	spike_loop_duration	iter=1 loop=1	38628
1057822029	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	344
1057822059	ui	session_end	finish success=yes loop=1
1057865747	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1057867563	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2215
1057869593	worker	spike_loop_gap	after_loop=1 gap_us=47967	47967
1057924073	worker	spike_loop_duration	iter=0	54478
1057929069	ui	ui_fast_repeat_flush	pending_iters=1	384
1057930999	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1928
1057939437	ui	session_end	finish success=yes
1059210616	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1059213378	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3255
1059214925	worker	spike_loop_gap	after_loop=0 gap_us=1290850	1290850
1059215288	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1059215690	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1059252291	worker	spike_loop_duration	iter=0	37362
1059252874	ui	ui_fast_repeat_flush	pending_iters=1	415
1059329744	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4158
1059329926	ui	session_end	finish success=yes
1061103720	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
1061106307	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	3048
1061107847	worker	spike_loop_gap	after_loop=0 gap_us=1855556	1855556
1061108263	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1061108649	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1061145627	worker	spike_loop_duration	iter=0	37777
1061146277	ui	ui_fast_repeat_flush	pending_iters=1	409
1061200055	worker	loop_interval_sleep	requested_ms=53	54306
1061200063	worker	spike_loop_gap	after_loop=0 gap_us=54435	54435
1061215982	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e loop=1	1029
1061241355	worker	spike_loop_duration	iter=1 loop=1	41288
1061241761	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	350
1061242433	ui	session_end	finish success=yes loop=1
1063876010	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1063878594	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3079
1063880337	worker	spike_loop_gap	after_loop=1 gap_us=2638981	2638981
1063880975	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1063881394	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1063918769	worker	spike_loop_duration	iter=0	38427
1063919426	ui	ui_fast_repeat_flush	pending_iters=1	417
1063981046	worker	loop_interval_sleep	requested_ms=61	62156
1063981052	worker	spike_loop_gap	after_loop=0 gap_us=62285	62285
1063999187	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1122
1064018958	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	72
1064032660	worker	spike_loop_duration	iter=1 loop=1	51606
1064036991	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	301
1064036995	ui	session_end	finish success=yes loop=1
1064146100	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	12
1064159984	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1064161909	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2956
1064163360	worker	spike_loop_gap	after_loop=1 gap_us=130699	130699
1064205094	worker	spike_loop_duration	iter=0	41730
1064205820	ui	ui_fast_repeat_flush	pending_iters=1	543
1064273402	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1005
1064277831	ui	session_end	finish success=yes
1064321104	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1064322959	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2253
1064324502	worker	spike_loop_gap	after_loop=0 gap_us=119407	119407
1064361992	worker	spike_loop_duration	iter=0	37487
1064362726	ui	ui_fast_repeat_flush	pending_iters=1	417
1064433280	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1108
1064440997	ui	session_end	finish success=yes
1064486942	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1064489402	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3130
1064491815	worker	spike_loop_gap	after_loop=0 gap_us=129819	129819
1064532841	worker	spike_loop_duration	iter=0	41020
1064538871	ui	ui_fast_repeat_flush	pending_iters=1	674
1064587260	worker	loop_interval_sleep	requested_ms=53	54317
1064587267	worker	spike_loop_gap	after_loop=0 gap_us=54427	54427
1064601665	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1574
1064631655	worker	spike_loop_duration	iter=1 loop=1	44386
1064632092	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	345
1064632141	ui	session_end	finish success=yes loop=1
1065191797	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1065194514	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3096
1065195986	worker	spike_loop_gap	after_loop=1 gap_us=564329	564329
1065196564	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1065197166	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1065236227	worker	spike_loop_duration	iter=0	40240
1065237643	ui	ui_fast_repeat_flush	pending_iters=1	550
1065296489	worker	loop_interval_sleep	requested_ms=59	60216
1065296496	worker	spike_loop_gap	after_loop=0 gap_us=60267	60267
1065304703	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	935
1065340064	worker	spike_loop_duration	iter=1 loop=1	43566
1065341892	ui	session_end	reason=preempted_by_new_session loop=1
1065341976	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1065345558	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4800
1065387865	worker	spike_loop_duration	iter=0	40521
1065388947	ui	ui_fast_repeat_flush	pending_iters=1	477
1065443009	worker	loop_interval_sleep	requested_ms=54	54981
1065443015	worker	spike_loop_gap	after_loop=0 gap_us=55150	55150
1065747488	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	303952
1065757516	ui	session_end	reason=preempted_by_new_session loop=1
1065757566	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1065759573	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2317
1065759957	ui	session_end	reason=preempted_by_new_session
1065760011	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1065762454	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2874
1065762777	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	319
1065762939	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	160
1065763768	ui	session_end	reason=preempted_by_new_session
1065763817	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
1065765628	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2687
1065765920	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	287
1065773016	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1065773476	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1065775919	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1065776399	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1065778294	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1065778621	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1065779227	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	52
1065784289	worker	spike_synthetic_key	dur_us=341261	341261
1065784304	worker	spike_loop_duration	iter=1	341288
1065788695	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	51
1065788728	ui	ui_fast_repeat_flush	pending_iters=1	23
1065788734	ui	session_end	finish success=yes
1065895809	ui	session_begin	feature=R mode=Hold profile=LOL source=hotkey blocks=1
1065897538	ui	hotkey_hold_start_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	2039
1065897597	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4
1065897599	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1
1065897917	ui	hotkey_hold_end_dispatch	ed7b7b8e-e3a2-4280-a958-2a4a666b634e	318
1065899141	worker	spike_loop_gap	after_loop=1 gap_us=114835	114835
1065914275	ui	session_end	reason=preempted_by_new_session
1065914367	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1065917930	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	4327
1065924122	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1065924604	ui	refreshWorkflowEditor	elapsed_ms=5	5000
1065925476	ui	session_end	reason=preempted_by_new_session
1065925529	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1065929084	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4012
1065936662	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1065937087	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1065937871	ui	session_end	reason=preempted_by_new_session
1065937931	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1065941683	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4184
1065956760	worker	spike_loop_duration	iter=0	57615
1066006131	worker	spike_synthetic_key	dur_us=82570	82570
1066006154	worker	spike_loop_duration	iter=0	82618
1066007013	worker	spike_loop_duration	iter=0	72897
1066007719	worker	spike_loop_duration	iter=0	60533
1066010418	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1066010797	ui	refreshWorkflowEditor	elapsed_ms=0	0
1066011495	ui	ui_fast_repeat_flush	pending_iters=1	10
1066011498	ui	session_end	finish success=yes
1066052712	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3720
1066056298	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3584
1066066571	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2047
1066118840	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1066122015	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3651
1066122396	ui	session_end	reason=preempted_by_new_session
1066122451	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1066125843	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3820
1066131729	ui	session_end	reason=preempted_by_new_session
1066131787	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1066134735	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4166
1066143029	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1066143394	ui	refreshWorkflowEditor	elapsed_ms=0	0
1066148190	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1066148530	ui	refreshWorkflowEditor	elapsed_ms=0	0
1066445333	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	296133
1066447797	worker	spike_synthetic_key	dur_us=323999	323999
1066447817	worker	spike_loop_duration	iter=0	324036
1066449014	worker	spike_synthetic_key	dur_us=320887	320887
1066449025	worker	spike_loop_duration	iter=0	320914
1066499236	worker	spike_synthetic_key	dur_us=362856	362856
1066499260	worker	spike_loop_duration	iter=0	362896
1066506711	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	902
1066507530	ui	session_end	finish success=yes
1066542180	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	35467
1066543336	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1066546353	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4170
1066547693	ui	session_end	reason=preempted_by_new_session
1066547746	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1066550121	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3760
1066550413	ui	session_end	reason=preempted_by_new_session
1066550463	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1066553050	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2923
1066553534	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	478
1066553911	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	372
1066554616	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	703
1066554672	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	54
1066554701	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	28
1066554729	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	27
1066554788	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2
1066554789	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	0
1066554791	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	0
1066576151	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1066576538	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1066578125	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1066578578	ui	refreshWorkflowEditor	elapsed_ms=0	0
1066580535	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1066581001	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1066581878	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	84
1066581938	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	58
1066581999	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	59
1066598259	worker	spike_loop_duration	iter=0	50183
1066599272	ui	ui_fast_repeat_flush	pending_iters=1	11
1066599275	ui	session_end	finish success=yes
1066634811	worker	spike_synthetic_key	dur_us=83235	83235
1066635965	worker	spike_synthetic_key	dur_us=81387	81387
1066687278	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	6
1066687281	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1
1066687283	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1
1068554489	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1068560335	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	6350
1068561990	worker	spike_loop_gap	after_loop=0 gap_us=1963728	1963728
1068562656	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1068563079	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1068599757	worker	spike_loop_duration	iter=0	37763
1068600385	ui	ui_fast_repeat_flush	pending_iters=1	432
1068658183	worker	loop_interval_sleep	requested_ms=57	58322
1068658191	worker	spike_loop_gap	after_loop=0 gap_us=58434	58434
1068673713	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	4513
1068700274	worker	spike_loop_duration	iter=1 loop=1	42080
1068700653	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	311
1068700711	ui	session_end	finish success=yes loop=1
1068744331	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1068746142	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2111
1068747633	worker	spike_loop_gap	after_loop=1 gap_us=47358	47358
1068800803	worker	spike_loop_duration	iter=0	53168
1068809214	ui	session_end	finish success=yes
1068844022	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	38531
1068858024	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1068862255	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4767
1068864579	worker	spike_loop_gap	after_loop=0 gap_us=63772	63772
1068902597	worker	spike_loop_duration	iter=0	38011
1068903704	ui	ui_fast_repeat_flush	pending_iters=1	840
1068974133	worker	loop_interval_sleep	requested_ms=70	71382
1068974139	worker	spike_loop_gap	after_loop=0 gap_us=71544	71544
1068984861	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1341
1069009948	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	118
1069012453	worker	spike_loop_duration	iter=1 loop=1	38313
1069013106	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	560
1069013153	ui	session_end	finish success=yes loop=1
1069117212	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	6
1069181976	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1069185092	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3597
1069187247	worker	spike_loop_gap	after_loop=1 gap_us=174792	174792
1069227222	worker	spike_loop_duration	iter=0	39968
1069227823	ui	ui_fast_repeat_flush	pending_iters=1	404
1069295789	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	899
1069301067	ui	session_end	finish success=yes
1069349532	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1069352137	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3057
1069353694	worker	spike_loop_gap	after_loop=0 gap_us=126474	126474
1069391114	worker	spike_loop_duration	iter=0	37416
1069391691	ui	ui_fast_repeat_flush	pending_iters=1	411
1069463418	worker	loop_interval_sleep	requested_ms=71	72208
1069463425	worker	spike_loop_gap	after_loop=0 gap_us=72311	72311
1069768028	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	303424
1069787783	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	66
1069787786	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2
1069800637	worker	spike_synthetic_key	dur_us=337190 loop=1	337190
1069800652	worker	spike_loop_duration	iter=1 loop=1	337225
1069801967	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	1154
1069802310	ui	session_end	finish success=yes loop=1
1069969940	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1069972815	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3379
1069978223	worker	spike_loop_gap	after_loop=1 gap_us=177568	177568
1070015439	worker	spike_loop_duration	iter=0	37214
1070016240	ui	ui_fast_repeat_flush	pending_iters=1	457
1070071690	worker	loop_interval_sleep	requested_ms=55	56112
1070071696	worker	spike_loop_gap	after_loop=0 gap_us=56257	56257
1070095218	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1845
1070122591	worker	spike_loop_duration	iter=1 loop=1	50892
1070123185	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	524
1070123232	ui	session_end	finish success=yes loop=1
1070166407	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1070168298	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2206
1070169919	worker	spike_loop_gap	after_loop=1 gap_us=47327	47327
1070225481	worker	spike_loop_duration	iter=0	55558
1070230499	ui	session_end	finish success=yes
1070264623	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	34728
1070295194	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1070297661	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3028
1070299273	worker	spike_loop_gap	after_loop=0 gap_us=73790	73790
1070339211	worker	spike_loop_duration	iter=0	39935
1070340674	ui	ui_fast_repeat_flush	pending_iters=1	865
1070400352	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1452
1070405242	ui	session_end	finish success=yes
1070448758	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1070450437	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2156
1070452271	worker	spike_loop_gap	after_loop=0 gap_us=113059	113059
1070490208	worker	spike_loop_duration	iter=0	37936
1070491201	ui	ui_fast_repeat_flush	pending_iters=1	551
1070543296	worker	loop_interval_sleep	requested_ms=52	53040
1070543303	worker	spike_loop_gap	after_loop=0 gap_us=53093	53093
1070556421	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	957
1070586728	worker	spike_loop_duration	iter=1 loop=1	43424
1070587095	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	325
1070587121	ui	session_end	finish success=yes loop=1
1070627158	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1070630013	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3335
1070631618	worker	spike_loop_gap	after_loop=1 gap_us=44888	44888
1070690994	worker	spike_loop_duration	iter=0	59374
1070695357	ui	ui_fast_repeat_flush	pending_iters=1	659
1070705241	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1277
1070711389	ui	session_end	finish success=yes
1073088240	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1073090485	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2776
1073091923	worker	spike_loop_gap	after_loop=0 gap_us=2400927	2400927
1073092912	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1073093367	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1073130646	worker	spike_loop_duration	iter=0	38721
1073131653	ui	ui_fast_repeat_flush	pending_iters=1	757
1073193064	worker	loop_interval_sleep	requested_ms=61	62315
1073193070	worker	spike_loop_gap	after_loop=0 gap_us=62424	62424
1073205427	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1078
1073232269	worker	spike_loop_duration	iter=1 loop=1	39197
1073236352	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	324
1073236356	ui	session_end	finish success=yes loop=1
1073962152	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1073964970	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3332
1073969957	worker	spike_loop_gap	after_loop=1 gap_us=737684	737684
1073970594	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1073971074	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1074008115	worker	spike_loop_duration	iter=0	38153
1074008811	ui	ui_fast_repeat_flush	pending_iters=1	535
1074060392	worker	loop_interval_sleep	requested_ms=51	52179
1074060398	worker	spike_loop_gap	after_loop=0 gap_us=52284	52284
1074074952	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1145
1074105250	worker	spike_loop_duration	iter=1 loop=1	44849
1074105659	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	348
1074105692	ui	session_end	finish success=yes loop=1
1074156815	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1074159127	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2720
1074160933	worker	spike_loop_gap	after_loop=1 gap_us=55683	55683
1074230898	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1142
1074245407	worker	spike_synthetic_key	dur_us=84447	84447
1074245514	worker	spike_loop_duration	iter=0	84577
1074246530	ui	ui_fast_repeat_flush	pending_iters=1	739
1074246534	ui	session_end	finish success=yes
1074359402	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1074361660	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2569
1074363191	worker	spike_loop_gap	after_loop=0 gap_us=117676	117676
1074397307	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1149
1074402795	worker	spike_loop_duration	iter=0	39602
1074403607	ui	ui_fast_repeat_flush	pending_iters=1	588
1074403610	ui	session_end	finish success=yes
1074494528	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1074496406	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2223
1074498278	worker	spike_loop_gap	after_loop=0 gap_us=95478	95478
1074537641	worker	spike_loop_duration	iter=0	39359
1074538115	ui	ui_fast_repeat_flush	pending_iters=1	324
1074606137	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	962
1074610387	ui	session_end	finish success=yes
1074653219	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1074655232	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2314
1074656746	worker	spike_loop_gap	after_loop=0 gap_us=119104	119104
1074694153	worker	spike_loop_duration	iter=0	37404
1074694729	ui	ui_fast_repeat_flush	pending_iters=1	401
1074745341	worker	loop_interval_sleep	requested_ms=50	51082
1074745347	worker	spike_loop_gap	after_loop=0 gap_us=51195	51195
1074760890	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1009
1074772672	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	75
1074790739	worker	spike_loop_duration	iter=1 loop=1	45389
1074791397	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	486
1074791402	ui	session_end	finish success=yes loop=1
1074904502	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	8
1074935755	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1074937879	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2443
1074939550	worker	spike_loop_gap	after_loop=1 gap_us=148810	148810
1074979890	worker	spike_loop_duration	iter=0	40336
1074980898	ui	ui_fast_repeat_flush	pending_iters=1	562
1075038384	worker	loop_interval_sleep	requested_ms=57	58364
1075038390	worker	spike_loop_gap	after_loop=0 gap_us=58501	58501
1075342949	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	302461
1075374957	worker	spike_synthetic_key	dur_us=336549 loop=1	336549
1075374971	worker	spike_loop_duration	iter=1 loop=1	336579
1075375355	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	331
1075375387	ui	session_end	finish success=yes loop=1
1075607346	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1075610672	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3816
1075612252	worker	spike_loop_gap	after_loop=1 gap_us=237280	237280
1075612828	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1075613213	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1075649982	worker	spike_loop_duration	iter=0	37725
1075650797	ui	ui_fast_repeat_flush	pending_iters=1	416
1075731487	worker	loop_interval_sleep	requested_ms=80	81408
1075731492	worker	spike_loop_gap	after_loop=0 gap_us=81512	81512
1076034917	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	302552
1076048095	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	52
1076048099	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	2
1076068168	worker	spike_synthetic_key	dur_us=336658 loop=1	336658
1076068186	worker	spike_loop_duration	iter=1 loop=1	336692
1076069155	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	875
1076069461	ui	session_end	finish success=yes loop=1
1080795955	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1080799102	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3618
1080801548	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
1080801628	worker	spike_loop_gap	after_loop=1 gap_us=4733441	4733441
1080802366	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1080839648	worker	spike_loop_duration	iter=0	38017
1080840275	ui	ui_fast_repeat_flush	pending_iters=1	464
1080905121	worker	loop_interval_sleep	requested_ms=64	65375
1080905128	worker	spike_loop_gap	after_loop=0 gap_us=65480	65480
1080918428	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1022
1080948256	worker	spike_loop_duration	iter=1 loop=1	43125
1080948839	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	505
1080948873	ui	session_end	finish success=yes loop=1
1080992628	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1080994705	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2511
1080996219	worker	spike_loop_gap	after_loop=1 gap_us=47962	47962
1081052726	worker	spike_loop_duration	iter=0	56504
1081059444	ui	ui_fast_repeat_flush	pending_iters=1	426
1081068149	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1808
1081073473	ui	session_end	finish success=yes
1081170211	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1081173738	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4031
1081175475	worker	spike_loop_gap	after_loop=0 gap_us=122745	122745
1081209353	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1238
1081221587	worker	spike_loop_duration	iter=0	46107
1081222564	ui	ui_fast_repeat_flush	pending_iters=1	707
1081222569	ui	session_end	finish success=yes
1081324522	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1081326706	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2507
1081328295	worker	spike_loop_gap	after_loop=0 gap_us=106706	106706
1081367709	worker	spike_loop_duration	iter=0	39412
1081368627	ui	ui_fast_repeat_flush	pending_iters=1	731
1081377928	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1148
1081380542	ui	session_end	finish success=yes
1081481895	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1081483791	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2231
1081485472	worker	spike_loop_gap	after_loop=0 gap_us=117759	117759
1081523902	worker	spike_loop_duration	iter=0	38425
1081526543	ui	ui_fast_repeat_flush	pending_iters=1	435
1081537512	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1072
1081544347	ui	session_end	finish success=yes
1084179507	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1084182700	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3746
1084184972	worker	spike_loop_gap	after_loop=0 gap_us=2661072	2661072
1084185450	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1084185901	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1084225847	worker	spike_loop_duration	iter=0	40872
1084226518	ui	ui_fast_repeat_flush	pending_iters=1	443
1084281338	worker	loop_interval_sleep	requested_ms=54	55418
1084281347	worker	spike_loop_gap	after_loop=0 gap_us=55499	55499
1084311321	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1131
1084322223	worker	spike_loop_duration	iter=1 loop=1	40873
1084322640	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	358
1084322736	ui	session_end	finish success=yes loop=1
1085197606	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1085200281	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3141
1085202027	worker	spike_loop_gap	after_loop=1 gap_us=879803	879803
1085202402	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1085202843	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1085240073	worker	spike_loop_duration	iter=0	38042
1085240824	ui	ui_fast_repeat_flush	pending_iters=1	496
1085307363	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	841
1085314883	ui	session_end	finish success=yes
1085381719	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1085385575	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4342
1085387269	worker	spike_loop_gap	after_loop=0 gap_us=147194	147194
1085426435	worker	spike_loop_duration	iter=0	39164
1085427315	ui	ui_fast_repeat_flush	pending_iters=1	415
1085498367	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1068
1085503828	ui	session_end	finish success=yes
1089748556	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1089751567	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3511
1089753175	worker	spike_loop_gap	after_loop=0 gap_us=4326739	4326739
1089753717	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1089754436	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1089791823	worker	spike_loop_duration	iter=0	38643
1089792402	ui	ui_fast_repeat_flush	pending_iters=1	420
1089863197	worker	loop_interval_sleep	requested_ms=70	71277
1089863203	worker	spike_loop_gap	after_loop=0 gap_us=71382	71382
1089876196	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1406
1089901864	worker	spike_loop_duration	iter=1 loop=1	38657
1089902420	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	456
1089902496	ui	session_end	finish success=yes loop=1
1090013425	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1090015490	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2383
1090017498	worker	spike_loop_gap	after_loop=1 gap_us=115632	115632
1090022862	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1090023277	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1090057326	worker	spike_loop_duration	iter=0	39823
1090059599	ui	ui_fast_repeat_flush	pending_iters=1	484
1090115860	worker	loop_interval_sleep	requested_ms=57	58408
1090115869	worker	spike_loop_gap	after_loop=0 gap_us=58544	58544
1090139093	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1017
1090169283	worker	spike_loop_duration	iter=1 loop=1	53411
1090170029	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	484
1090170033	ui	session_end	finish success=yes loop=1
1091713092	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1091715475	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2920
1091716700	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1091718022	worker	spike_loop_gap	after_loop=1 gap_us=1548738	1548738
1091718106	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1091756175	worker	spike_loop_duration	iter=0	38150
1091756730	ui	ui_fast_repeat_flush	pending_iters=1	386
1091811530	worker	loop_interval_sleep	requested_ms=54	55259
1091811536	worker	spike_loop_gap	after_loop=0 gap_us=55361	55361
1091838522	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27 loop=1	1054
1091852888	worker	spike_loop_duration	iter=1 loop=1	41350
1091853333	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	380
1091853452	ui	session_end	finish success=yes loop=1
1092859276	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1092862657	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4114
1092864515	worker	spike_loop_gap	after_loop=1 gap_us=1011626	1011626
1092864815	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
1092865243	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1092902359	worker	spike_loop_duration	iter=0	37838
1092902991	ui	ui_fast_repeat_flush	pending_iters=1	423
1092955642	worker	loop_interval_sleep	requested_ms=52	53170
1092955649	worker	spike_loop_gap	after_loop=0 gap_us=53292	53292
1093000554	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1553
1093011841	worker	spike_loop_duration	iter=1 loop=1	56190
1093012291	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	342
1093012324	ui	session_end	finish success=yes loop=1
1095351308	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1095353634	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3041
1095355289	worker	spike_loop_gap	after_loop=1 gap_us=2343447	2343447
1095355663	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1095356054	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1095393001	worker	spike_loop_duration	iter=0	37708
1095393592	ui	ui_fast_repeat_flush	pending_iters=1	419
1095463845	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1123
1095468361	ui	session_end	finish success=yes
1095516626	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1095518720	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3156
1095520393	worker	spike_loop_gap	after_loop=0 gap_us=127392	127392
1095557116	worker	spike_loop_duration	iter=0	36718
1095558042	ui	ui_fast_repeat_flush	pending_iters=1	475
1095632823	worker	loop_interval_sleep	requested_ms=74	75611
1095632828	worker	spike_loop_gap	after_loop=0 gap_us=75714	75714
1095938440	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	304207
1095969723	worker	spike_synthetic_key	dur_us=336876 loop=1	336876
1095969827	worker	spike_loop_duration	iter=1 loop=1	336997
1095970508	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	363
1095970513	ui	session_end	finish success=yes loop=1
1099136028	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1099138387	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2687
1099140253	worker	spike_loop_gap	after_loop=1 gap_us=3170424	3170424
1099176634	worker	spike_loop_duration	iter=0	36379
1099177427	ui	ui_fast_repeat_flush	pending_iters=1	425
1099245277	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	920
1099249389	ui	session_end	finish success=yes
1099317362	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1099319791	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2952
1099321319	worker	spike_loop_gap	after_loop=0 gap_us=144683	144683
1099358728	worker	spike_loop_duration	iter=0	37405
1099359353	ui	ui_fast_repeat_flush	pending_iters=1	415
1099429642	worker	loop_interval_sleep	requested_ms=69	70807
1099429649	worker	spike_loop_gap	after_loop=0 gap_us=70921	70921
1099732962	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	299799
1099744553	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	54
1099744557	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	2
1099744585	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	26
1099744587	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1
1099766194	worker	spike_synthetic_key	dur_us=336524 loop=1	336524
1099766211	worker	spike_loop_duration	iter=1 loop=1	336560
1099767376	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	1101
1099767746	ui	session_end	finish success=yes loop=1
1099869890	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1099871662	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2093
1099873393	worker	spike_loop_gap	after_loop=1 gap_us=107180	107180
1099912950	worker	spike_loop_duration	iter=0	39552
1099913674	ui	ui_fast_repeat_flush	pending_iters=1	539
1099920871	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1373
1099923422	ui	session_end	finish success=yes
1100018870	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1100021948	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3458
1100024129	worker	spike_loop_gap	after_loop=0 gap_us=111176	111176
1100065515	worker	spike_loop_duration	iter=0	41382
1100066071	ui	ui_fast_repeat_flush	pending_iters=1	396
1100128428	worker	loop_interval_sleep	requested_ms=61	62822
1100128435	worker	spike_loop_gap	after_loop=0 gap_us=62920	62920
1100139345	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1134
1100168295	worker	spike_loop_duration	iter=1 loop=1	39857
1100169530	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	484
1100169535	ui	session_end	finish success=yes loop=1
1100210315	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1100212104	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2121
1100213717	worker	spike_loop_gap	after_loop=1 gap_us=45421	45421
1100272307	worker	spike_loop_duration	iter=0	58588
1100278357	ui	ui_fast_repeat_flush	pending_iters=1	422
1100290321	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	860
1100293359	ui	session_end	finish success=yes
1100391374	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1100393528	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2486
1100395183	worker	spike_loop_gap	after_loop=0 gap_us=122873	122873
1100436153	worker	spike_loop_duration	iter=0	40968
1100437084	ui	ui_fast_repeat_flush	pending_iters=1	460
1100498093	worker	loop_interval_sleep	requested_ms=61	61894
1100498099	worker	spike_loop_gap	after_loop=0 gap_us=61945	61945
1100512491	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1014
1100536733	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	48
1100542455	worker	spike_loop_duration	iter=1 loop=1	44353
1100542929	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	408
1100543192	ui	session_end	finish success=yes loop=1
1100649729	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	5
1100704332	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1100708355	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4620
1100710504	worker	spike_loop_gap	after_loop=1 gap_us=168048	168048
1100751466	worker	spike_loop_duration	iter=0	40958
1100752095	ui	ui_fast_repeat_flush	pending_iters=1	445
1100804734	worker	loop_interval_sleep	requested_ms=52	53171
1100804742	worker	spike_loop_gap	after_loop=0 gap_us=53278	53278
1100825517	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	926
1100856091	worker	spike_loop_duration	iter=1 loop=1	51347
1100856488	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	337
1100856522	ui	session_end	finish success=yes loop=1
1100901971	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1100903984	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2319
1100905571	worker	spike_loop_gap	after_loop=1 gap_us=49477	49477
1100962780	worker	spike_loop_duration	iter=0	57207
1100968790	ui	ui_fast_repeat_flush	pending_iters=1	404
1100977575	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1115
1100985724	ui	session_end	finish success=yes
1101080904	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1101084250	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3711
1101086416	worker	spike_loop_gap	after_loop=0 gap_us=123633	123633
1101128949	worker	spike_loop_duration	iter=0	42529
1101130513	ui	ui_fast_repeat_flush	pending_iters=1	745
1101198273	worker	loop_interval_sleep	requested_ms=68	69152
1101198279	worker	spike_loop_gap	after_loop=0 gap_us=69330	69330
1101502657	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	302841
1101513677	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	51
1101513681	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1
1101537449	worker	spike_synthetic_key	dur_us=339155 loop=1	339155
1101537462	worker	spike_loop_duration	iter=1 loop=1	339181
1101538678	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	701
1101538683	ui	session_end	finish success=yes loop=1
1104218423	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1104221655	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3748
1104223255	worker	spike_loop_gap	after_loop=1 gap_us=2685792	2685792
1104224317	ui	WorkflowEditorPanel.refresh	elapsed_ms=1	1000
1104224760	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1104261736	worker	spike_loop_duration	iter=0	38479
1104262525	ui	ui_fast_repeat_flush	pending_iters=1	454
1104318095	worker	loop_interval_sleep	requested_ms=55	56247
1104318105	worker	spike_loop_gap	after_loop=0 gap_us=56367	56367
1104353230	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e loop=1	1374
1104377300	worker	spike_loop_duration	iter=1 loop=1	59193
1104378009	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	631
1104378177	ui	session_end	finish success=yes loop=1
1105904843	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1105907614	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3276
1105909293	worker	spike_loop_gap	after_loop=1 gap_us=1531992	1531992
1105946120	worker	spike_loop_duration	iter=0	36821
1105946930	ui	ui_fast_repeat_flush	pending_iters=1	399
1106015531	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1009
1106018875	ui	session_end	finish success=yes
1106118704	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1106122051	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	4141
1106123615	worker	spike_loop_gap	after_loop=0 gap_us=177497	177497
1106124254	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106124726	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1106164231	worker	spike_loop_duration	iter=0	40612
1106165006	ui	ui_fast_repeat_flush	pending_iters=1	462
1106220288	worker	loop_interval_sleep	requested_ms=55	55927
1106220295	worker	spike_loop_gap	after_loop=0 gap_us=56066	56066
1106233208	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16 loop=1	1906
1106262538	worker	spike_loop_duration	iter=1 loop=1	42240
1106262945	ui	ui_fast_repeat_flush	pending_iters=1 loop=1	345
1106262990	ui	session_end	finish success=yes loop=1
1106314992	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1106316824	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2136
1106317191	ui	session_end	reason=preempted_by_new_session
1106317236	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1106320571	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3743
1106375077	worker	spike_loop_duration	iter=0	56402
1106376460	worker	spike_loop_duration	iter=0	54541
1106378209	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106378674	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1106380450	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106380957	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1106385751	ui	session_end	finish success=yes
1106422153	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	36922
1106424326	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2170
1106496006	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1106498287	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2602
1106498562	ui	session_end	reason=preempted_by_new_session
1106498625	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1106502169	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3876
1106510469	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106510886	ui	refreshWorkflowEditor	elapsed_ms=0	0
1106542935	worker	spike_loop_duration	iter=0	42736
1106543114	ui	ui_fast_repeat_flush	pending_iters=1	25
1106545823	worker	spike_loop_duration	iter=0	41424
1106548852	ui	ui_fast_repeat_flush	pending_iters=1	1766
1106550718	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1859
1106556998	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1149
1106561323	ui	session_end	finish success=yes
1106663842	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1106665735	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2234
1106667557	worker	spike_loop_gap	after_loop=0 gap_us=121731	121731
1106672041	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106672474	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1106680815	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1113
1106706557	worker	spike_loop_duration	iter=0	38996
1106707414	ui	ui_fast_repeat_flush	pending_iters=1	656
1106707417	ui	session_end	finish success=yes
1106807998	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1106811213	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3613
1106811778	ui	session_end	reason=preempted_by_new_session
1106811876	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1106816438	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	5214
1106817022	ui	session_end	reason=preempted_by_new_session
1106817083	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1106821528	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	5009
1106831927	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106832490	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1106835650	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106836253	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1106839439	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106840210	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1106843665	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2775
1106846482	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	2811
1106857410	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1237
1106868246	worker	spike_loop_duration	iter=0	54188
1106869254	worker	spike_loop_duration	iter=0	50129
1106870236	worker	spike_loop_duration	iter=0	46201
1106870424	ui	ui_fast_repeat_flush	pending_iters=1	18
1106870428	ui	session_end	finish success=yes
1106973944	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1106975814	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2214
1106976178	ui	session_end	reason=preempted_by_new_session
1106976226	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1106979285	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3463
1106979756	ui	session_end	reason=preempted_by_new_session
1106979811	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1106982605	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	3311
1106990669	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106991153	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1106992591	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106993140	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1106995322	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1106995647	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1107297600	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	297826
1107596485	worker	spike_synthetic_key	dur_us=618958	618958
1107596516	worker	spike_loop_duration	iter=0	619008
1107897116	worker	spike_synthetic_key	dur_us=916094	916094
1107897146	worker	spike_loop_duration	iter=0	916144
1107898853	ui	session_end	finish success=yes
1107937377	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	639773
1107939244	worker	spike_synthetic_key	dur_us=954999	954999
1108000287	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2396
1108001276	ui	session_begin	feature=Q mode=Hold profile=LOL source=hotkey blocks=1
1108004274	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	3983
1108004675	ui	session_end	reason=preempted_by_new_session
1108004726	ui	session_begin	feature=W mode=Hold profile=LOL source=hotkey blocks=1
1108008180	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	3897
1108008658	ui	session_end	reason=preempted_by_new_session
1108008716	ui	session_begin	feature=E mode=Hold profile=LOL source=hotkey blocks=1
1108013121	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	4931
1108013526	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	396
1108013693	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	166
1108013840	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	144
1108014333	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	493
1108014748	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	410
1108014804	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	51
1108014834	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	2
1108014836	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	2
1108014869	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	33
1108014994	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	32
1108014996	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1
1108014997	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	1
1108014998	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	1
1108015026	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	28
1108015053	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	26
1108015081	ui	hotkey_hold_start_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	27
1108015082	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	0
1108015084	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1
1108015084	ui	hotkey_hold_end_dispatch	dc0c8f42-674f-4b11-abe5-28e5835e940e	0
1108015111	ui	hotkey_hold_start_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	26
1108015148	ui	hotkey_hold_start_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	31
1108015149	ui	hotkey_hold_end_dispatch	5e310cf3-a12e-4bc4-9004-a90ce0704c27	1
1108015149	ui	hotkey_hold_end_dispatch	b3748a91-67a7-4185-a7f3-f17fd0483c16	0
1108027322	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1108027742	ui	refreshWorkflowEditor	elapsed_ms=2	2000
1108029173	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1108029576	ui	refreshWorkflowEditor	elapsed_ms=0	0
1108030833	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1108031139	ui	refreshWorkflowEditor	elapsed_ms=0	0
1108059131	worker	spike_loop_duration	iter=0	53015
1108059399	ui	ui_fast_repeat_flush	pending_iters=1	27
1108059403	ui	session_end	finish success=yes
1108099258	worker	spike_synthetic_key	dur_us=89056	89056
1108100021	worker	spike_synthetic_key	dur_us=85229	85229
1109982777	ui	switchToProfile.maybeSave	elapsed_ms=0	0
1109982959	ui	switchToProfile.saveSettings	elapsed_ms=0	0
1109983225	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
1109983316	ui	switchToProfile.stopSessions	elapsed_ms=0	0
1109984271	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1109984625	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1109984786	ui	refreshWorkflowEditor	elapsed_ms=0	0
1109985069	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1109985233	ui	refreshWorkflowEditor	elapsed_ms=0	0
1109985322	ui	syncHotkeys	elapsed_ms=0	0
1109985393	ui	loadProjectFromFile	elapsed_ms=1	1000
1109985421	ui	loadActiveProfile	elapsed_ms=1	1000
1109985430	ui	switchToProfile.loadActiveProfile	elapsed_ms=1	1000
1109988102	ui	switchToProfile	elapsed_ms=5	5000
1109988128	ui	profile_switch	id=1f4a4367-387b-4984-9019-1b6fc9ad35d8 auto=yes	5672
1110135803	ui	switchToProfile.maybeSave	elapsed_ms=0	0
1110135956	ui	switchToProfile.saveSettings	elapsed_ms=0	0
1110136252	ui	stopAllSessionsForProfileSwitch	elapsed_ms=0	0
1110136286	ui	switchToProfile.stopSessions	elapsed_ms=0	0
1110137287	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1110142214	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1110142472	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1110143937	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1110144126	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1110144378	ui	syncHotkeys	elapsed_ms=0	0
1110144501	ui	loadProjectFromFile	elapsed_ms=7	7000
1110144548	ui	loadActiveProfile	elapsed_ms=7	7000
1110144559	ui	switchToProfile.loadActiveProfile	elapsed_ms=7	7000
1110150503	ui	switchToProfile	elapsed_ms=15	15000
1110150545	ui	profile_switch	id=497ab714-9a83-4b14-ab8c-e9d90ba4a663 auto=yes	15250
1110161099	ui	session_begin	feature=수락 mode=Trigger profile=LOL source=restore blocks=2
1110162283	ui	WorkflowEditorPanel.refresh	elapsed_ms=0	0
1110162585	ui	refreshWorkflowEditor	elapsed_ms=1	1000
1110163199	ui	trigger_monitor_start	block=#1
1110163410	ui	session_end	reason=preempted_by_new_session
1110163496	ui	session_begin	feature=올리폿 mode=Trigger profile=LOL source=restore blocks=52
1110167638	ui	WorkflowEditorPanel.refresh	elapsed_ms=2	2000
1110168022	ui	refreshWorkflowEditor	elapsed_ms=4	4000
1110170094	ui	trigger_monitor_start	block=#1
1111592671	worker	spike_loop_duration	iter=0	1428068
1111592672	worker	spike_loop_duration	iter=0	1421100
1111594920	ui	auto_save	quiet=no	1100
1111619594	ui	profile_package_seal	sync=yes	24532
1111620489	ui	session_end	finish success=yes
1111663323	ui	app_trace_end	app_shutdown
```
