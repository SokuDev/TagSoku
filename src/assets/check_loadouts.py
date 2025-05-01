import os
import json
import traceback

for folder in sorted(os.listdir()):
	try:
		if not os.path.isdir(folder):
			continue
		with open(os.path.join(folder, 'loadouts.json')) as fd:
			loadouts = json.load(fd)
		configs = [
			(lambda d: (json.load(d), d.close())[0])(
				open(os.path.join(folder, f'config{i}.json'))
			) for i in range(3)
		]
		for i in range(3):
			loadout = loadouts[i]
			config = configs[i]

			if loadout[0] not in config["spells"]:
				print(f"Loadout {i} for {folder}: doesn't contain the spell in the spell list")
			if loadout[4] != config["shownCost"]:
				print(f"Loadout {i} for {folder}: character select loadout cost is wrong")

			if folder == "tewi":
				airName    = config["5s0"]["air"]["action"]    - config["5s0"]["air"]["cardName"]
				groundName = config["5s0"]["ground"]["action"] - config["5s0"]["ground"]["cardName"]
				if groundName != 400 and groundName != 450 and groundName != 420 and groundName != 470:
					print(f"Loadout {i} for {folder}: Displayed card name for ground is wrong")
				if airName != 400 and airName != 450 and airName != 420 and airName != 470:
					print(f"Loadout {i} for {folder}: Displayed card name for air is wrong")
				airName    = config["5s1"]["air"]["action"]    - config["5s1"]["air"]["cardName"]
				groundName = config["5s1"]["ground"]["action"] - config["5s1"]["ground"]["cardName"]
				if groundName != 400 and groundName != 450 and groundName != 420 and groundName != 470:
					print(f"Loadout {i} for {folder}: Displayed card name for ground is wrong")
				if airName != 400 and airName != 450 and airName != 420 and airName != 470:
					print(f"Loadout {i} for {folder}: Displayed card name for air is wrong")
				if config["shownCost"] != config["5s0"]["ground"]["cost"]:
					print(f"Loadout {i} for {folder}: shownCost for ground is wrong")
				if config["shownCost"] != config["5s1"]["ground"]["cost"]:
					print(f"Loadout {i} for {folder}: shownCost for ground is wrong")
				if config["shownCost"] != config["5s0"]["air"]["cost"]:
					print(f"Loadout {i} for {folder}: shownCost for air is wrong")
				if config["shownCost"] != config["5s1"]["air"]["cost"]:
					print(f"Loadout {i} for {folder}: shownCost for air is wrong")
			else:
				airName    = config["5s"]["air"]["action"]    - config["5s"]["air"]["cardName"]
				groundName = config["5s"]["ground"]["action"] - config["5s"]["ground"]["cardName"]
				if groundName != 400 and groundName != 450:
					print(f"Loadout {i} for {folder}: Displayed card name for ground is wrong")
				if airName != 400 and airName != 450:
					print(f"Loadout {i} for {folder}: Displayed card name for air is wrong")
				if config["shownCost"] != config["5s"]["ground"]["cost"]:
					print(f"Loadout {i} for {folder}: shownCost for ground is wrong")
				if config["shownCost"] != config["5s"]["air"]["cost"]:
					print(f"Loadout {i} for {folder}: shownCost for air is wrong")

			nbSkills = 4
			if folder == "kaguya" or folder == "patchouli":
				nbSkills = 5

			if abs(loadout[1] - loadout[2]) % nbSkills == 0:
				print(f"Loadout {i} for {folder}: Skill 1 and 2 are on the same input")
			if abs(loadout[2] - loadout[3]) % nbSkills == 0:
				print(f"Loadout {i} for {folder}: Skill 2 and 3 are on the same input")
			if abs(loadout[1] - loadout[3]) % nbSkills == 0:
				print(f"Loadout {i} for {folder}: Skill 1 and 3 are on the same input")
			if folder == "tewi":
				names = ["8a0", "8b0", "8c0", "8a1", "8b1", "8c1"]
				for j in range(6):
					action = 500 + (loadout[j % 3 + 1] - 100) * 8
					aAction = config[names[j]]["air"]["action"]
					gAction = config[names[j]]["ground"]["action"]
					if names[j].endswith('0'):
						action += 4
					if not (action <= gAction < action + 4):
						print(f"Loadout {i} for {folder}: {names[j]} Ground action doesn't match skill")
					if not (action <= aAction < action + 4):
						print(f"Loadout {i} for {folder}: {names[j]} Air action doesn't match skill")
			else:
				names = ["8a", "8b", "8c"]
				for j in range(3):
					alt = (loadout[j + 1] - 100) // nbSkills
					skill = (loadout[j + 1] - 100) % nbSkills
					action = 500 + 20 * skill + alt * 5
					aAction = config[names[j]]["air"]["action"]
					gAction = config[names[j]]["ground"]["action"]
					if not (action <= gAction < action + 5):
						print(f"Loadout {i} for {folder}: {names[j]} Ground action doesn't match skill")
					if not (action <= aAction < action + 5):
						print(f"Loadout {i} for {folder}: {names[j]} Air action doesn't match skill")
	except:
		print(folder)
		traceback.print_exc()
