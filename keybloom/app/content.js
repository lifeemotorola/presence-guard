/*
 * KeyBloom — original lesson and practice text.
 * Copyright (C) 2026 Emmanuel Suah
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.KeyBloomContent = factory();
})(typeof globalThis === 'undefined' ? this : globalThis, function () {
  'use strict';

  const lessons = [
    { id: 'home-fj', group: 'Home row', title: 'Find your anchors', keys: 'F  J', minutes: 2,
      tip: 'Rest your index fingers on the little bumps on F and J. Let your thumbs share the space bar.',
      text: 'f j f j ff jj fj jf f j jf fj ff jj f j fj jf' },
    { id: 'home-dk', group: 'Home row', title: 'Meet D and K', keys: 'D  K', minutes: 2,
      tip: 'Your middle fingers reach D and K. Return your hands to the home row after every key.',
      text: 'd k d k dk kd fd jk df kj dk fd jk dd kk fd jk' },
    { id: 'home-sl', group: 'Home row', title: 'Add S and L', keys: 'S  L', minutes: 2,
      tip: 'Use your ring fingers for S and L. Aim for a relaxed rhythm, not speed.',
      text: 's l s l sl ls sf jl sk ld sj fl ds kl sl ls' },
    { id: 'home-a-semicolon', group: 'Home row', title: 'The outside keys', keys: 'A  ;', minutes: 2,
      tip: 'Your little fingers take A and the semicolon. Keep the rest of your fingers relaxed.',
      text: 'a ; a ; as la sad lad ask flask salad fall; a; as; la;' },
    { id: 'home-gh', group: 'Home row', title: 'Stretch to G and H', keys: 'G  H', minutes: 2,
      tip: 'Reach inward with your index fingers for G and H, then return to F and J.',
      text: 'g h fg jh gf hj glad half flag dash glass hall glad half' },
    { id: 'home-words', group: 'Home row', title: 'Home row rhythm', keys: 'HOME ROW', minutes: 3,
      tip: 'Keep your eyes on the screen. It is fine to slow down while your hands learn the pattern.',
      text: 'a sad lad asks; a lass falls; all shall fall; a flask; a salad; a lad asks' },
    { id: 'top-ru', group: 'Top row', title: 'Reach for R and U', keys: 'R  U', minutes: 2,
      tip: 'Move each index finger up one row, then bring it back to F or J.',
      text: 'r u fr ju rf uj fur jar far rug guard fur jar r u fr ju' },
    { id: 'top-ty', group: 'Top row', title: 'Reach for T and Y', keys: 'T  Y', minutes: 2,
      tip: 'Stretch each index finger to the centre of the top row, then return to F or J.',
      text: 't y ft jy tf yj try yard dusty tardy rust stay dry truly' },
    { id: 'top-ei', group: 'Top row', title: 'Easy E and I', keys: 'E  I', minutes: 2,
      tip: 'Reach E and I with your middle fingers. Keep your wrists loose.',
      text: 'e i de ki ed ik ride fire idea sure rise fair ride fire' },
    { id: 'top-wo', group: 'Top row', title: 'Welcome W and O', keys: 'W  O', minutes: 2,
      tip: 'Your ring fingers move up for W and O, then come back to S and L.',
      text: 'w o sw lo ws ol slow flower world rose word lower slower' },
    { id: 'top-qp', group: 'Top row', title: 'The top row edges', keys: 'Q  P', minutes: 2,
      tip: 'Reach gently with your little fingers for Q and P; there is no need to stretch hard.',
      text: 'q p aq ;p qa p; quiet paper power quite equal people' },
    { id: 'top-words', group: 'Top row', title: 'Put the rows together', keys: 'TOP + HOME', minutes: 3,
      tip: 'Try a steady pace. A small pause is better than looking down at every key.',
      text: 'we all see a quiet lake; a soft fire glows; water ripples past a wide shore' },
    { id: 'bottom-vm', group: 'Bottom row', title: 'Down to V and M', keys: 'V  M', minutes: 2,
      tip: 'Your index fingers reach down to V and M. Return to F and J afterwards.',
      text: 'v m fv jm vf mj move wave warm room very vivid move' },
    { id: 'bottom-bn', group: 'Bottom row', title: 'Bring in B and N', keys: 'B  N', minutes: 2,
      tip: 'Your index fingers reach toward the centre for B and N. Come back to the home row.',
      text: 'b n fb jn bf nj bean brown begin number bring bunny' },
    { id: 'bottom-c-comma', group: 'Bottom row', title: 'C and comma', keys: 'C  ,', minutes: 2,
      tip: 'Use the middle fingers for C and comma. The comma lives just below K.',
      text: 'c , dc k, cd ,k calm, clear, music, magic, calm, clear' },
    { id: 'bottom-x-period', group: 'Bottom row', title: 'X and full stop', keys: 'X  .', minutes: 2,
      tip: 'X and the full stop are reached by the ring fingers. Try not to rush the punctuation.',
      text: 'x . sx l. xs .l mix. relax. next. extra. mix. relax.' },
    { id: 'bottom-z-slash', group: 'Bottom row', title: 'Z and slash', keys: 'Z  /', minutes: 2,
      tip: 'The little fingers take Z and slash. Stay comfortable and bring them back home.',
      text: 'z / az ;/ za /; zero. zoom. zebra. jazz. yes/no. up/down.' },
    { id: 'shift', group: 'Everyday typing', title: 'Make it a capital', keys: 'SHIFT', minutes: 3,
      tip: 'Hold Shift with the opposite hand when you can. Release it after the capital letter.',
      text: 'A new day begins. We learn one key at a time. Practice makes progress.' },
    { id: 'numbers', group: 'Everyday typing', title: 'Numbers in reach', keys: '0–9', minutes: 3,
      tip: 'Move only the finger you need toward the number row, then return to the home row.',
      text: '123 456 789 0. 10 minutes a day, 7 days a week. 2026 is a fresh start.' },
    { id: 'punctuation', group: 'Everyday typing', title: 'Add expression', keys: "' ? !", minutes: 3,
      tip: 'Take your time with apostrophes, question marks and exclamation points.',
      text: "What's your next goal? I'm learning, one word at a time! Let's keep going." },
    { id: 'everyday', group: 'Everyday typing', title: 'A little everyday practice', keys: 'ALL KEYS', minutes: 3,
      tip: 'Let your hands do the work. Accuracy comes first; speed will follow with practice.',
      text: 'Small steps add up. Take a breath, sit comfortably, and let each word flow onto the page.' },
  ];

  const passages = [
    'Every new skill begins with a small step. Give your hands time to find their rhythm, and celebrate the progress you make today.',
    'A bright morning is a good time to learn something new. A few minutes of patient practice can make tomorrow feel easier.',
    'The best pace is one you can repeat comfortably. Sit tall, relax your shoulders, and let your fingers return to the home row.',
    'On a quiet street, a small garden grows beside an open window. Green leaves turn toward the light as the day begins.',
    'Writing clearly helps ideas travel. Whether you are sending a message or planning a project, each word has a place.',
    'A steady rhythm matters more than a perfect first try. Notice where you hesitate, take a breath, and try that part again.',
    'Learning is not a race against anyone else. Measure your progress against your own starting point, one session at a time.',
    'A thoughtful question can open a door. Listen carefully, share your ideas, and leave room for someone else to add theirs.',
  ];

  const sprintPassages = [
    'bloom little bright leaf garden light calm learn steady happy green warm quick focus gentle',
    'river mango paper cloud small smile fresh home start grow kind music open clear today',
    'story friend quiet path sunny voice place dream skill simple brave word spark create',
    'morning breeze flower train water step guide heart window nature play circle shine',
    'orange smile ready world pause level rhythm listen lesson journey soft faster future',
  ];

  return { lessons, passages, sprintPassages };
});
