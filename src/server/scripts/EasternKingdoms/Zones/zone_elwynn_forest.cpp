/*
 * Copyright (C) 2016 DeathCore <http://www.noffearrdeathproject.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptedEscortAI.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "World.h"
#include "PetAI.h"
#include "PassiveAI.h"
#include "CombatAI.h"
#include "GameEventMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SpellAuras.h"
#include "Vehicle.h"
#include "Player.h"
#include "SpellScript.h"

enum Northshire
{
    NPC_BLACKROCK_BATTLE_WORG = 49871,
    NPC_STORMWIND_INFANTRY    = 49869,
    NPC_BROTHER_PAXTON        = 951,
    NPC_INJURED_SOLDIER_DUMMY = 50378,
	
    SAY_YELL                  = 1,

    SPELL_SPYING              = 92857,
    SPELL_SNEAKING            = 93046,
    SPELL_SPYGLASS            = 80676,

    SPELL_RENEW               = 93094,
    SPELL_PRAYER_OF_HEALING   = 93091,
    SPELL_FORTITUDE           = 13864,
    SPELL_PENANCE             = 47750,
    SPELL_FLASH_HEAL          = 17843,
    SPELL_RENEWEDLIFE         = 93097,

    ACTION_HEAL               = 1,
    EVENT_HEALED_1            = 1,
    EVENT_HEALED_2            = 2	
};

/*######
## npc_stormwind_infantry
######*/

class npc_stormwind_infantry : public CreatureScript
{
public:
    npc_stormwind_infantry() : CreatureScript("npc_stormwind_infantry") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_stormwind_infantryAI (creature);
    }

    struct npc_stormwind_infantryAI : public ScriptedAI
    {
        npc_stormwind_infantryAI(Creature* creature) : ScriptedAI(creature) {}

        bool HasATarget;
		
        uint32 tYell, SayChance, WillSay;

        void Reset()
        {
            HasATarget = false;
            WillSay     = urand(0,100);
            SayChance   = 25;			
            tYell       = urand(10000, 20000);
        }

        void DamageTaken(Unit* doneBy, uint32& damage)
        {
            if (doneBy->ToCreature())
                if (me->GetHealth() <= damage || me->GetHealthPct() <= 80.0f)
                    damage = 0;
        }

        void DamageDealt(Unit* target, uint32& damage, DamageEffectType damageType)
        {
            if (target->ToCreature())
                if (target->GetHealth() <= damage || target->GetHealthPct() <= 70.0f)
                    damage = 0;
        }

        void MoveInLineOfSight(Unit* who)
        {
            if (who && !HasATarget)
                if (me->GetDistance(who) < 5.0f)
                    if (Creature* creature = who->ToCreature())
                        if (creature->GetEntry() == NPC_BLACKROCK_BATTLE_WORG)
                            AttackStart(who);
        }

        void EnterEvadeMode()
        {
            HasATarget = false;
        }
		
        void UpdateAI(uint32 diff)
        {
            if(tYell <= diff)
            {
                if (WillSay <= SayChance)
                {			   
                    Talk(SAY_YELL);
                    tYell = urand(10000, 20000);          
                }			   
            }
            else tYell -= diff;

            if (!UpdateVictim())
                return;
            else
                DoMeleeAttackIfReady();
        }
    };
};

/*######
## npc_blackrock_battle_worg
######*/

class npc_blackrock_battle_worg : public CreatureScript
{
public:
    npc_blackrock_battle_worg() : CreatureScript("npc_blackrock_battle_worg") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_blackrock_battle_worgAI (creature);
    }

    struct npc_blackrock_battle_worgAI : public ScriptedAI
    {
        npc_blackrock_battle_worgAI(Creature* creature) : ScriptedAI(creature) { }

        void DamageTaken(Unit* who, uint32& damage)
        {
            if (who->GetEntry() == NPC_STORMWIND_INFANTRY && damage >= me->GetHealth())
                me->SetHealth(me->GetMaxHealth());

            if (who->GetTypeId() == TYPEID_PLAYER || who->IsPet())
            {
                if (Creature* guard = me->FindNearestCreature(NPC_STORMWIND_INFANTRY, 6.0f, true))
                {
                    guard->getThreatManager().resetAllAggro();
                    guard->CombatStop(true);
                }

                me->getThreatManager().resetAllAggro();
                me->GetMotionMaster()->MoveChase(who);
                me->AI()->AttackStart(who);
            }
        }

        void UpdateAI(uint32 diff)
        {
            if (!UpdateVictim())
                if (Creature* guard = me->FindNearestCreature(NPC_STORMWIND_INFANTRY, 6.0f, true))
                {
                    me->SetReactState(REACT_AGGRESSIVE);
                    me->AI()->AttackStart(guard);
                }
            DoMeleeAttackIfReady();
        }
    };
};

class npc_stormwind_injured_soldier : public CreatureScript
{
public:
    npc_stormwind_injured_soldier() : CreatureScript("npc_stormwind_injured_soldier") { }

    struct npc_stormwind_injured_soldierAI : public ScriptedAI
    {
        npc_stormwind_injured_soldierAI(Creature* creature) : ScriptedAI(creature) { }

        EventMap events;
        Player* owner;

        void Reset()
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_15);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_14);
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }

        void SpellHit(Unit* caster, SpellInfo const* spell)
        {
            if (spell->Id == SPELL_RENEWEDLIFE)
            {
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_15);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_14);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                DoAction(ACTION_HEAL);
                owner = me->FindNearestPlayer(8.0f, true);
            }
        }

        void DoAction(int32 action)
        {
            switch (action)
            {
            case ACTION_HEAL:
                events.ScheduleEvent(EVENT_HEALED_1, 2000);
                break;
            }
        }

        void UpdateAI(uint32 diff)
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_HEALED_1:
                    if (owner)
                    {
                        switch(urand(0, 5))
                        {
                        case 0:
                            me->MonsterSay("I will fear no evil!", 0, NULL);
                            break;
                        case 1:
                            me->MonsterSay("I... I'm okay! I'm okay!", 0, NULL);
                            break;
                        case 2:
                            me->MonsterSay("Bless you, hero!", 0, NULL);
                            break;
                        case 3:
                            me->MonsterSay("You are $p! The hero that everyone has been talking about! Thank you!", 0, owner);
                            break;
                        case 4:
                            me->MonsterSay("Thank the Light!", 0, NULL);
                            break;
                        case 5:
                            me->MonsterSay("I live to fight another day!", 0, NULL);
                            break;
                        default:
                            break;
                        }
                        me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
                        owner->KilledMonsterCredit(me->GetEntry(), NULL);
                        events.ScheduleEvent(EVENT_HEALED_2, 2500);
                    }
                    break;
                case EVENT_HEALED_2:
                    me->GetMotionMaster()->MovePoint(0, -8914.525f, -133.963f, 80.534f);
                    me->DespawnOrUnsummon(8000);
                    break;
                default:
                    break;
                }
            }
        }
    };
    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_stormwind_injured_soldierAI (creature);
    }
};

enum Marshal
{
    QUEST_REPORT_TO_GOLDSHIRE           = 54,
    SAY_DISMISSED                       = 0
};

/*######
## npc_marshal_mcbride
######*/

class npc_marshal_mcbride : public CreatureScript
{
public:
    npc_marshal_mcbride() : CreatureScript("npc_marshal_mcbride") { }

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest)
    {
        if (quest->GetQuestId() == QUEST_REPORT_TO_GOLDSHIRE)
        {
            creature->AI()->Talk(SAY_DISMISSED, player);
        }
        return true;
    }

};

/*######
## npc_hogger
######*/

enum Spells
{
    SPELL_EATING           = 87351,
    SPELL_VICIOUS_SLICE    = 87337,
    SPELL_SUMMON_MINIONS   = 87366,
    SPELL_SIMPLY_TELEPORT  = 64446,
};

class npc_hogger : public CreatureScript
{
public:
    npc_hogger() : CreatureScript("npc_hogger") { }

    CreatureAI* GetAI(Creature* pCreature) const
    {
        return new npc_hoggerAI(pCreature);
    }

    struct npc_hoggerAI : public ScriptedAI
    {
        uint8 phase;

        uint32 m_uiViciousSliceTimer;
        uint32 m_uiSummonMinions;
        uint32 m_uiPhaseTimer;
        uint32 m_uiDespawnTimer;

        uint64 GeneralGUID;
        uint64 Mage1GUID;
        uint64 Mage2GUID;
        uint64 Raga1GUID;
        uint64 Raga2GUID;
        uint64 PlayerGUID;

        bool bSay;
        bool bSay2;
        bool bSay3;
        bool Summon;
        bool bSummoned;
        bool bSummoned3;
        bool bGo1;
        bool bCasted;
        bool Credit;

        npc_hoggerAI(Creature *c) : ScriptedAI(c) {}

        void Reset()
        {
            me->RestoreFaction();
            me->RemoveFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_NON_ATTACKABLE);

            m_uiViciousSliceTimer = 8000;
            m_uiSummonMinions = 15000;
            m_uiPhaseTimer = 0;
            m_uiDespawnTimer = 500;

            phase = 0;
            GeneralGUID = 0;
            Mage1GUID = 0;
            Mage2GUID = 0;
            Raga1GUID = 0;
            Raga2GUID = 0;
            PlayerGUID = 0;

            Summon = false;
            bSay = false;
            bCasted = false;
            bSay2 = false;
            bSay3 = false;
            bSummoned = false;
            bSummoned3 = false;
            bGo1 = false; 
            Credit = false;
        }

        void JustDied(Unit* /*killer*/)
        {
            me->RestoreFaction();
        }

        void AttackedBy(Unit* pAttacker)
        {
            if (me->GetVictim() || me->IsFriendlyTo(pAttacker))
                return;

            AttackStart(pAttacker);
        }

        void MovementInform(uint32 type, uint32 id)
        {
            if (id == 0)
            {
                DoCast(me, SPELL_EATING);
            }
        }

        void UpdateAI(uint32 diff)
        {
            ScriptedAI::UpdateAI(diff);

            DoMeleeAttackIfReady();

            if (m_uiViciousSliceTimer <= diff)
            {
                DoCast(me->GetVictim(), SPELL_VICIOUS_SLICE);
                m_uiViciousSliceTimer = 10000;
            } else
            m_uiViciousSliceTimer -= diff;

            if (HealthBelowPct(50))
            {
                if(!bSummoned)
                {
                    DoCast(me->GetVictim(), SPELL_SUMMON_MINIONS);
                    bSummoned = true;
                }
                if(!bSay)
                {
                    me->MonsterYell("Yipe! Help Hogger", 0, NULL);
                    bSay=true;
                }    
            }

            if(HealthBelowPct(35))
            {
                if(!bSay2)
                {
                    me->MonsterTextEmote("Hogger is eating! Stop him!", NULL, true);
                    bSay2=true;
                }
                if(!bGo1)
                {    
                    me->AttackStop();
                    me->SetReactState(REACT_PASSIVE);
                    me->GetMotionMaster()->MovePoint(0, -10142.081f, 671.773f, 36.014f);
                    bGo1 = true;
                }
            }

            if(HealthBelowPct(10))
            {
                if(!bSummoned3)
                {
                    if(!bSay3)
                    {
                        me->MonsterYell("No hurt Hogger!", 0, NULL);
                        bSay3=true;
                    }

                    me->CombatStop(true);
                    me->SetFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_NON_ATTACKABLE);

                    Creature* General = me->SummonCreature(46942, -10133.275f, 663.244f, 35.964616f, 2.45f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 180000);
                    GeneralGUID = General->GetGUID();

                    Creature* Mage1 = me->SummonCreature(46941, -10129.976f, 667.982f, 35.67f, 2.85f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 180000);
                    Mage1GUID = Mage1->GetGUID();

                    Creature* Mage2 = me->SummonCreature(46940, -10137.671f, 659.926f, 35.971f, 2.051f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 180000);
                    Mage2GUID = Mage2->GetGUID();

                    Creature* Raga1 = me->SummonCreature(46943, -10133.339f, 660.087f, 35.971f, 2.26f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 180000);
                    Raga1GUID = Raga1->GetGUID();

                    Creature* Raga2 = me->SummonCreature(42413, -10129.461f, 663.180f, 35.9491f, 2.37f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 180000);
                    Raga2GUID = Raga2->GetGUID();

                    Summon = true;

                    m_uiPhaseTimer = 1500;
                    phase = 1;
                }

                if(Summon)
                {
                    Creature* General = Unit::GetCreature(*me, GeneralGUID);
                    Creature* Raga2 = Unit::GetCreature(*me, Raga2GUID);
                    Creature* Raga1 = Unit::GetCreature(*me, Raga1GUID);
                    Creature* Mage2 = Unit::GetCreature(*me, Mage2GUID);
                    Creature* Mage1 = Unit::GetCreature(*me, Mage1GUID);

                    if(!bCasted)
                    {
                        General->CastSpell(General,64446,true);
                        Raga2->CastSpell(Raga2,64446,true);
                        Raga1->CastSpell(Raga1,64446,true);
                        Mage2->CastSpell(Raga2,64446,true);
                        Mage1->CastSpell(Raga1,64446,true);
                        bCasted = true;
                    }

                    bSummoned3=true;
                    me->SetSpeed(MOVE_RUN, 1.0f);
                }
            }

            if(bCasted)
            {
                if (m_uiPhaseTimer <= diff)
                {
                    Creature* General = Unit::GetCreature(*me, GeneralGUID);
                    Creature* Raga2 = Unit::GetCreature(*me, Raga2GUID);
                    Creature* Raga1 = Unit::GetCreature(*me, Raga1GUID);
                    Creature* Mage2 = Unit::GetCreature(*me, Mage2GUID);
                    Creature* Mage1 = Unit::GetCreature(*me, Mage1GUID);

                    switch(phase)
                    {

                        case 1: me->GetMotionMaster()->MovePoint(1, -10141.054f, 670.719f, 35.9569f); m_uiPhaseTimer = 3000; phase = 2; break;
                        case 2: General->MonsterYell("Hold your blade, adventurer!", 0, NULL); m_uiPhaseTimer = 2500; phase = 3; break;
                        case 3: Raga1->MonsterSay("WoW!", 0, NULL); m_uiPhaseTimer = 1500; phase = 4; break;
                        case 4: Raga2->MonsterSay("General Jonathan Marcus!", 0, NULL); m_uiPhaseTimer = 1500; phase = 5; break;
                        case 5:
                        {
                            if(Creature* General =  Unit::GetCreature(*me, GeneralGUID))
                            {
                                General->MonsterSay("This beast leads the Riverpaw gnoll gang and may be the key to ending gnoll aggression in Elwynn!", 0, NULL);
                                General->AddUnitMovementFlag(MOVEMENTFLAG_WALKING);
                                General->GetMotionMaster()->MovePoint(0, -10137.162f, 667.919f, 35.937f);
                                m_uiPhaseTimer = 3000;
                            }
                            phase = 6;
                        } break;
                        case 6: me->MonsterSay("Grrr...", 0, NULL); m_uiPhaseTimer = 4000; phase = 7; break;
                        case 7:
                        {
                            General->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
                            m_uiPhaseTimer = 4500;
                            phase = 8;
                        } break;

                        case 8: General->MonsterSay("We're taking him into custody in the name of King Varian Wrynn.", 0, NULL); m_uiPhaseTimer = 4000; phase = 9; break;
                        case 9: me->MonsterSay("Nooooo...", 0, NULL); m_uiPhaseTimer = 4000; phase = 10; break;
                        case 10: General->MonsterSay("Take us to the Stockades, Andromath.", 0, NULL); m_uiPhaseTimer = 4000; phase = 11; break;
                                General->SetOrientation(6.08f);
                        case 11:
                        {
                            General->CastSpell(General,87459,true);
                            Raga1->CastSpell(Raga1,87459,true);
                            Raga2->CastSpell(Raga2,87459,true);
                            Mage1->CastSpell(Mage1,64446,true);
                            Mage2->CastSpell(Mage2,87459,true);
                            me->CastSpell(me,87459,true);

                            General->DespawnOrUnsummon(500);
                            Raga1->DespawnOrUnsummon(500);
                            Raga2->DespawnOrUnsummon(500);
                            Mage1->DespawnOrUnsummon(500);
                            Mage2->DespawnOrUnsummon(500);
                            me->DespawnOrUnsummon(500);

                            std::list<Player*> players;

                            Trinity::AnyPlayerInObjectRangeCheck checker(me, 35.0f);
                            Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(me, players, checker);
                            me->VisitNearbyWorldObject(35.0f, searcher);

                            for (std::list<Player*>::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                                (*itr)->KilledMonsterCredit(448, NULL);

                            phase = 0;
                        } break;
                        default: break;
                    }
                } else m_uiPhaseTimer -= diff;
            }
            if (me->GetHealth() <= 1)
            { 
                if (m_uiDespawnTimer <= diff)
                {
                    me->CombatStop(true);
                    me->AttackStop();
                    me->ClearAllReactives();
                    me->DeleteThreatList();
                    me->SetHealth(me->GetMaxHealth());                       
                } else m_uiDespawnTimer -= diff;
            }
        }  
        void DamageTaken(Unit* done_by, uint32 & damage)
        {
            if (PlayerGUID == 0)
            {
                if (Player *pPlayer = done_by->ToPlayer())
                {
                    PlayerGUID = pPlayer->GetGUID();
                }
            }

            if (me->GetHealth() <= damage)
            {
                damage = me->GetHealth() - 1; 

                if (Credit == false)
                {
                    me->RemoveAllAuras();
                    me->CombatStop(true);
                    me->AttackStop();
                    me->ClearAllReactives();
                    me->DeleteThreatList();

                }Credit = true;
            }
        }
   };
};

/*######
## npc_training_dummy_elwynn
######*/

enum eTrainingDummySpells
{
    SPELL_CHARGE        = 100,
    SPELL_AUTORITE      = 105361, // OnDamage
    SPELL_ASSURE        = 56641,
    SPELL_EVISCERATION  = 2098,
    SPELL_MOT_DOULEUR_1 = 589,
    SPELL_MOT_DOULEUR_2 = 124464, // Je ne sais pas si un des deux est le bon
    SPELL_NOVA          = 122,
    SPELL_CORRUPTION_1  = 172,
    SPELL_CORRUPTION_2  = 87389,
    SPELL_CORRUPTION_3  = 131740,
    SPELL_PAUME_TIGRE   = 100787
};

class npc_training_dummy_start_zones : public CreatureScript
{
public:
    npc_training_dummy_start_zones() : CreatureScript("npc_training_dummy_start_zones") { }

    struct npc_training_dummy_start_zonesAI : Scripted_NoMovementAI
    {
        npc_training_dummy_start_zonesAI(Creature* creature) : Scripted_NoMovementAI(creature)
        {}

        uint32 resetTimer;

        void Reset()
        {
            me->SetControlled(true, UNIT_STATE_STUNNED); // Disable rotate
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true); // Immune to knock always like blast wave

            resetTimer = 5000;
        }

        void EnterEvadeMode()
        {
            if (!_EnterEvadeMode())
                return;

            Reset();
        }

        void DamageTaken(Unit* doneBy, uint32& damage)
        {
            resetTimer = 5000;
            damage = 0;

            if (doneBy->HasAura(SPELL_AUTORITE))
                if (doneBy->ToPlayer())
                    doneBy->ToPlayer()->KilledMonsterCredit(44175, 0);
        }

        void EnterCombat(Unit* /*who*/)
        {
            return;
        }

        void SpellHit(Unit* Caster, const SpellInfo* Spell)
        {
            switch (Spell->Id)
            {
                case SPELL_CHARGE:
                case SPELL_ASSURE:
                case SPELL_EVISCERATION:
                case SPELL_MOT_DOULEUR_1:
                case SPELL_MOT_DOULEUR_2:
                case SPELL_NOVA:
                case SPELL_CORRUPTION_1:
                case SPELL_CORRUPTION_2:
                case SPELL_CORRUPTION_3:
                case SPELL_PAUME_TIGRE:
                    if (Caster->ToPlayer())
                        Caster->ToPlayer()->KilledMonsterCredit(44175, 0);
                    break;
                default:
                    break;
            }
        }

        void UpdateAI(uint32 diff)
        {
            if (!UpdateVictim())
                return;

            if (!me->HasUnitState(UNIT_STATE_STUNNED))
                me->SetControlled(true, UNIT_STATE_STUNNED); // Disable rotate

            if (resetTimer <= diff)
            {
                EnterEvadeMode();
                resetTimer = 5000;
            }
            else
                resetTimer -= diff;
        }
        void MoveInLineOfSight(Unit* /*who*/){return;}
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_training_dummy_start_zonesAI(creature);
    }
};

/*######
## spell_quest_fear_no_evil
######*/

class spell_quest_fear_no_evil : public SpellScriptLoader
{
    public:
        spell_quest_fear_no_evil() : SpellScriptLoader("spell_quest_fear_no_evil") { }

        class spell_quest_fear_no_evil_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_quest_fear_no_evil_SpellScript);

            void OnDummy(SpellEffIndex /*effIndex*/)
            {
                if (GetCaster())
                    if (GetCaster()->ToPlayer())
                        GetCaster()->ToPlayer()->KilledMonsterCredit(50047, 0);
            }

            void Register()
            {
                OnEffectHitTarget += SpellEffectFn(spell_quest_fear_no_evil_SpellScript::OnDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_quest_fear_no_evil_SpellScript();
        }
};

/*######
## spell_quest_extincteur
######*/

#define SPELL_VISUAL_EXTINGUISHER   96028

class spell_quest_extincteur : public SpellScriptLoader
{
    public:
        spell_quest_extincteur() : SpellScriptLoader("spell_quest_extincteur") { }

        class spell_quest_extincteur_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_quest_extincteur_AuraScript);

            void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (GetCaster())
                    GetCaster()->AddAura(SPELL_VISUAL_EXTINGUISHER, GetCaster());
            }

            void OnPeriodic(AuraEffect const* /*aurEff*/)
            {
                if (!GetCaster())
                    return;

                if (Creature* fire = GetCaster()->FindNearestCreature(42940, 5.0f, true))
                {
                    if (Player* player = GetCaster()->ToPlayer())
                        player->KilledMonsterCredit(42940, 0);

                    fire->DespawnOrUnsummon();
                }
            }

            void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
            {
                if (GetCaster())
                    GetCaster()->RemoveAurasDueToSpell(SPELL_VISUAL_EXTINGUISHER);
            }

            void Register()
            {
                OnEffectApply += AuraEffectApplyFn(spell_quest_extincteur_AuraScript::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_quest_extincteur_AuraScript::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
                OnEffectRemove += AuraEffectRemoveFn(spell_quest_extincteur_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_quest_extincteur_AuraScript();
        }
};

void AddSC_elwynn_forest()
{
    new npc_stormwind_infantry();
    new npc_blackrock_battle_worg();
    new npc_stormwind_injured_soldier();
    new npc_marshal_mcbride();
    new npc_hogger();
    new npc_training_dummy_start_zones();
    new spell_quest_fear_no_evil();
    new spell_quest_extincteur();
}
