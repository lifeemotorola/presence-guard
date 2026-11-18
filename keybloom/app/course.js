/*
 * KeyBloom — extended original practice library.
 * Copyright (C) 2026 Emmanuel Suah
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The 479 exercises below are deterministic and their IDs are stable. They
 * supplement, rather than replace, the first 21 lessons in content.js.
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.KeyBloomCourse = factory();
})(typeof globalThis === 'undefined' ? this : globalThis, function () {
  'use strict';

  // Each list contains words with the indicated letter; each sentence is
  // original and provides a more natural use of the same key.
  const letterSets = [
    ['a', 'apple anchor apron awake after paper garden travel always around', 'A calm afternoon allows a fresh start for anyone.'],
    ['b', 'bloom bright bubble basket bridge better about table number brave', 'Bright blue birds bring a bit of beauty to the branches.'],
    ['c', 'calm cloud circle candle ocean picnic music clear careful collect', 'Careful choices can create a calmer, clearer day.'],
    ['d', 'day dream dance garden dinner ready window steady middle decide', 'A good day begins with a clear plan and a steady mind.'],
    ['e', 'early eager even green gentle lesson breeze welcome little explore', 'Everyone can enjoy a gentle beginning to the evening.'],
    ['f', 'focus flower fresh friend forest future soft helpful safe effort', 'Fresh flowers fill the room with a soft, friendly scent.'],
    ['g', 'garden gentle grow green begin bright together gather morning great', 'Green leaves grow gently in the garden each morning.'],
    ['h', 'happy home hello heart helpful bright share healthy together cheerful', 'A helpful hand can make a hard task feel much lighter.'],
    ['i', 'idea island invite simple little bright listen kindness finish morning', 'I like to find simple ideas in the things around me.'],
    ['j', 'jazz journey joy jump jewel jacket joyful juice adjust project', 'Joy and laughter can make a long journey feel shorter.'],
    ['k', 'kind keyboard keep kitchen book market make walk look work', 'Keep looking for kind ways to help and encourage others.'],
    ['l', 'light learn little flower calm listen smile place follow lovely', 'A little light can make a familiar place feel lovely.'],
    ['m', 'morning music make smile moment dream home summer memory team', 'Morning music makes the room feel calm and welcoming.'],
    ['n', 'nature new number gentle green learn open window morning sunshine', 'New notes and a clear plan can help us begin again.'],
    ['o', 'open orange ocean focus flower home story tomorrow together morning', 'Open the door and look for something good in the day.'],
    ['p', 'paper practice path people happy open apple helpful project prepare', 'Patient practice helps people prepare for new projects.'],
    ['q', 'quiet quick queen question quote quilt equal square require sequence', 'A quiet question can lead to a quick and helpful answer.'],
    ['r', 'river ready rhythm orange garden learn bright morning friend create', 'A river runs around the rocks and toward the green trees.'],
    ['s', 'smile steady simple story sunshine listen music class glass rest', 'Small steps and steady practice can build lasting skills.'],
    ['t', 'time together today gentle bright little water start future practice', 'Take time to try a task, then take a moment to rest.'],
    ['u', 'useful under up music future cloud sunny journey community curious', 'A sunny afternoon is a useful time to enjoy the outdoors.'],
    ['v', 'value vivid visit view river move every brave arrive discover', 'A vivid view of the river rewards everyone who visits.'],
    ['w', 'window warm welcome water slow flower write word wonderful willow', 'Warm words and a welcoming smile can brighten the room.'],
    ['x', 'extra relax exact example mix box fox explore expand flexible', 'An extra example can help explain a complex idea.'],
    ['y', 'yellow young your yesterday happy early enjoy rhythm journey story', 'Your journey may feel easier when you try a little each day.'],
    ['z', 'zebra zero zone jazz maze prize dozen breeze frozen puzzle', 'A zebra rests in the shade while a breeze moves the grass.'],
  ];

  const punctuationSets = [
    ['period', 'Full stops', '.', [
      'The rain has stopped. A bright path appears beyond the gate.',
      'We finish one task. Then we begin the next one with care.',
      'The kettle is warm. The cups are ready on the table.',
      'A small seed rests in the soil. Soon a new leaf will appear.',
      'I read the note twice. The instructions are clear and simple.',
    ]],
    ['comma', 'Commas', ',', [
      'Bring paper, pencils, a notebook, and a little curiosity.',
      'After the rain, the garden looks fresh, green, and bright.',
      'Take a breath, relax your hands, and find a comfortable pace.',
      'On Monday, we cleaned the shelf, sorted the books, and rested.',
      'For lunch, choose a ripe mango, a warm roll, or a bowl of rice.',
    ]],
    ['question', 'Questions', '?', [
      'Where should we begin? Which task would be most helpful today?',
      'Have you checked the time? Can we meet after the lesson?',
      'What did you learn today? Is there a question you want to ask?',
      'Could you bring the notes? Would a short list make things easier?',
      'How does the garden look? Are the new leaves starting to grow?',
    ]],
    ['exclamation', 'Exclamation marks', '!', [
      'Well done! Your patient practice is beginning to pay off.',
      'What a lovely morning! The air feels fresh and cool.',
      'We finished the project! Thank you for working together.',
      'Great news! The books are ready for the community library.',
      'Keep going! Every careful attempt helps your fingers learn.',
    ]],
    ['apostrophe', 'Apostrophes', "'", [
      "I'm learning at my own pace, and that's a good place to start.",
      "We'll meet after lunch, but we won't need to hurry.",
      "Here's the list. It's short, clear, and ready to share.",
      "A friend's kind words can make a difficult day feel lighter.",
      "Let's take a break when we're ready, then try the next lesson.",
    ]],
    ['hyphen', 'Hyphens', '-', [
      'A well-planned day leaves room for rest and useful work.',
      'The blue-green leaves move softly in the afternoon breeze.',
      'Try a five-minute review before starting a new lesson.',
      'Our small-group meeting begins after the morning break.',
      'Keep an easy-to-read list of the steps for your project.',
    ]],
    ['colon', 'Colons', ':', [
      'For practice, gather three things: paper, a pencil, and time.',
      'Today has one goal: complete the task without rushing.',
      'The plan is simple: listen, learn, and ask useful questions.',
      'Please check the following: the date, the time, and the place.',
      'We need two items: a clean cup and a small spoon.',
    ]],
    ['semicolon', 'Semicolons', ';', [
      'The room is quiet; everyone is ready to begin.',
      'The path was long; we paused to enjoy the view.',
      'Practice takes patience; progress arrives one step at a time.',
      'The list is complete; the next task can now begin.',
      'Our plans may change; clear communication helps us adjust.',
    ]],
    ['slash', 'Slashes', '/', [
      'Choose yes/no for each question and review your answers.',
      'Use the folder path notes/project to find the practice files.',
      'The morning/afternoon schedule is posted near the door.',
      'Write a date in the form 04/12/2026 on the sample page.',
      'A simple on/off switch can make a tool easier to use.',
    ]],
    ['parentheses', 'Parentheses', '(', [
      'Please bring a notebook (or a few sheets of paper) tomorrow.',
      'The group will meet after lunch (around 2:00) in the hall.',
      'Pack a snack (fruit works well) before the journey begins.',
      'The practice session (about ten minutes) can be paused.',
      'Check the final page (including the short notes) before sharing.',
    ]],
  ];

  const everydaySets = [
    ['home', 'At home', [
      'Morning light falls across the table as the day begins.',
      'I fold the blanket and put the clean cups on the shelf.',
      'A quiet corner gives me space to read and think.',
      'We make a short list before tidying the room.',
      'The open window lets in a soft and gentle breeze.',
      'Fresh water and a clean cloth are ready by the sink.',
      'A favorite book rests beside the chair near the door.',
      'The room feels welcoming after everything is put away.',
      'We share a simple meal and talk about the day.',
      'At the end of the evening, the house becomes peaceful.',
    ]],
    ['study', 'Learning', [
      'A short review helps new ideas stay fresh in my mind.',
      'I write down a question before looking for an answer.',
      'The first attempt is a chance to notice what needs practice.',
      'A clear example can make a difficult idea easier to understand.',
      'We learn together by listening and sharing what we know.',
      'A useful notebook keeps important thoughts in one place.',
      'Taking a break can help me return with renewed attention.',
      'A little practice each day can build a lasting skill.',
      'I check my work slowly and correct one step at a time.',
      'Curiosity makes room for another question and another discovery.',
    ]],
    ['work', 'Working together', [
      'A clear plan helps the team understand the next step.',
      'We begin by listening to the ideas around the table.',
      'A helpful note can prevent confusion later in the day.',
      'Everyone has time to ask questions before the work starts.',
      'We divide a large task into smaller, manageable pieces.',
      'A short update lets the group know how things are going.',
      'A patient reply can make a busy conversation feel easier.',
      'The team checks the details before sharing the final draft.',
      'A well-organized workspace makes important tools easy to find.',
      'At the end of the project, we thank everyone who helped.',
    ]],
    ['nature', 'Nature', [
      'Green leaves turn toward the light after a gentle rain.',
      'A small bird rests on a branch above the quiet path.',
      'The river moves around stones and carries leaves downstream.',
      'A soft breeze passes through the tall grass by the road.',
      'New flowers appear where the soil has been carefully tended.',
      'The sky changes color as the evening light grows softer.',
      'A shady tree provides a cool place to pause and rest.',
      'The morning air feels fresh after a night of rain.',
      'Small seeds need time, water, and care before they grow.',
      'Clouds gather slowly while the sun warms the open ground.',
    ]],
    ['food', 'Food and cooking', [
      'Ripe fruit adds color to the table before breakfast.',
      'We rinse the vegetables and set them beside a clean bowl.',
      'The pot warms slowly while the rice is being prepared.',
      'A shared meal gives everyone a chance to talk and listen.',
      'I follow the recipe one step at a time.',
      'Fresh herbs bring a pleasant smell into the kitchen.',
      'A small spoon and a clean cup are ready on the counter.',
      'The market has many kinds of fruit, grains, and vegetables.',
      'We wash the dishes and put the kitchen tools away.',
      'A simple snack can be enjoyed during a quiet break.',
    ]],
    ['travel', 'Getting around', [
      'We check the route before leaving for the morning journey.',
      'A familiar landmark helps us know which way to turn.',
      'The road becomes quieter as we move beyond the busy market.',
      'A short rest gives everyone time to enjoy the view.',
      'I keep the travel notes in a place that is easy to find.',
      'The next stop is just beyond the bridge and the tall tree.',
      'We leave a little early so the journey can feel relaxed.',
      'A friendly greeting makes a new place feel more welcoming.',
      'The afternoon light stretches across the road ahead.',
      'At the end of the trip, we share stories about the day.',
    ]],
    ['community', 'Community', [
      'Neighbors gather to share ideas for a cleaner public space.',
      'A kind welcome helps new visitors feel part of the group.',
      'We listen to different views before choosing the next step.',
      'A shared calendar keeps everyone informed about the meeting.',
      'Small acts of care can make a neighborhood feel brighter.',
      'Volunteers arrange the chairs before the gathering begins.',
      'The library offers a quiet place to read and learn together.',
      'Clear signs help people find the entrance and the meeting room.',
      'Everyone can contribute a useful skill to the project.',
      'We celebrate the work that brings people closer together.',
    ]],
    ['creative', 'Creativity', [
      'A blank page can become the beginning of a new story.',
      'Colors and shapes can express an idea without any words.',
      'I save a few minutes to sketch an interesting object.',
      'A simple melody can make an ordinary moment feel special.',
      'The first draft does not have to be perfect to be useful.',
      'We arrange the materials before trying a new creative task.',
      'A small change can give a familiar design a fresh look.',
      'Ideas often grow when people share and build on them.',
      'An unfinished project can be revisited with a fresh eye.',
      'Making something by hand can be a calm way to learn.',
    ]],
    ['wellbeing', 'A comfortable pace', [
      'A relaxed posture can make a practice session feel easier.',
      'It is fine to slow down and focus on one key at a time.',
      'A short pause gives the hands a chance to rest.',
      'A glass of water is a welcome part of a quiet break.',
      'Comfort matters more than racing through every exercise.',
      'Take a breath and let the shoulders settle before typing.',
      'A steady rhythm is easier to repeat than a hurried one.',
      'There is value in noticing a small improvement today.',
      'Choose a comfortable seat and keep the workspace clear.',
      'A gentle routine can make learning feel more enjoyable.',
    ]],
    ['technology', 'Digital life', [
      'A clear file name makes a document easier to find later.',
      'I review a message before sending it to the group.',
      'A simple folder can keep related notes together.',
      'Saving a copy helps protect work from an accidental change.',
      'The keyboard offers a useful way to turn ideas into words.',
      'A short title helps a reader understand the topic.',
      'I close the window after finishing the task at hand.',
      'Organized notes can make a future project easier to begin.',
      'A careful search can help locate a document quickly.',
      'A few minutes of practice can make daily typing smoother.',
    ]],
  ];

  // Longer passages are individually written rather than repeating the short
  // sentence exercises. Each one offers a separate scene or practical idea.
  const paragraphSets = [
    ['home', 'Home routines', [
      'The sun reaches the doorway just as the kettle starts to sing. I open the curtains and make a list for the day. There is no need to hurry through breakfast.',
      'A loose button waits beside a needle and thread. With a few careful stitches, the shirt is ready to wear again. The small repair saves it from being set aside.',
      'We sort the old magazines into two piles: keep and share. A neighbor may enjoy the ones we have already read. The newly cleared shelf has room for a plant.',
      'When the power returns, I check the clock in the kitchen. It is ten minutes behind the time on my watch. I set it again and continue preparing supper.',
      'The chairs have gathered dust during the week. We wipe them down, sweep under the table, and open the door. Soon the room is ready for visitors.',
      'A pot of water warms on the stove while I prepare a cup. I add a spoon of tea and wait for the water to cool slightly. The warm drink makes a pleasant break.',
      'I keep the house keys in the same bowl every evening. This simple habit makes the morning easier. Before I leave, I check that the keys are in my bag.',
    ]],
    ['study', 'Learning and study', [
      'A difficult chapter becomes less intimidating when I divide it into sections. After reading each part, I write one sentence about it. Then I compare my notes with the text.',
      'Our teacher asks us to explain an idea without looking at the page. I pause, choose a familiar example, and describe it to a partner. The explanation shows me what to review.',
      'There are three new words on my list today. I learn their meanings and try using each one in a sentence. Tomorrow I will check whether I still remember them.',
      'The answer is not obvious at first, so I look back at the question. I underline the information that matters and cross out an unrelated detail. A smaller problem is easier to solve.',
      'My notebook has a date at the top of every page. Headings separate the questions from the answers. When it is time to review, I can find the right section quickly.',
      'A study group works best when everyone has a chance to speak. We take turns explaining a topic and asking for clarification. Each person leaves with a useful question to consider.',
      'I set a timer for a short reading session and place my phone out of reach. When the timer ends, I stand up and stretch. A break helps me return to the work with attention.',
    ]],
    ['work', 'Working together', [
      'The team has two possible approaches to the task. We list the benefits and limits of each one before deciding. A clear choice makes the next meeting more productive.',
      'A delivery has arrived earlier than expected. I check the items against the list and tell the coordinator what is missing. We can resolve the difference before the event begins.',
      'Our first plan needs a small change because the room is unavailable. We check the other spaces and choose one with enough chairs. Everyone receives the updated location.',
      'Before sharing the report, we read it aloud together. One sentence is confusing, and a number needs checking. A short review makes the final version clearer.',
      'The group is ready to begin a new project. We agree on a goal, name the first three tasks, and choose a date to check progress. Each person knows where to start.',
      'A colleague is away today, so I record the decisions from our discussion. The note includes the date and the action items. It will help them catch up when they return.',
      'We finish arranging the materials with a few minutes to spare. Instead of starting another task, we check the entrance and tidy the tables. The space is now ready for the group.',
    ]],
    ['nature', 'The outdoors', [
      'After a night of rain, drops cling to the edge of each leaf. A bird lands nearby and shakes the branch. For a moment, the ground sparkles in the morning light.',
      'The path bends around a patch of tall grass. We stop to watch an insect travel from one flower to another. Then we continue slowly toward the shade of a tree.',
      'A garden bed looks empty until the first green shoots appear. Each row has a small marker showing what was planted there. With water and care, the young plants begin to grow.',
      'At the edge of the water, smooth stones line the bank. The current moves gently between them, carrying a fallen leaf downstream. We sit for a while and listen.',
      'Clouds gather above the rooftops in the afternoon. A cool wind arrives before the first drops of rain. We collect the washing and move the chairs under shelter.',
      'The sky is still pale when we start our walk. As the light grows stronger, we notice tracks in the soft soil. We follow them only as far as the garden gate.',
      'A large tree offers shade to people waiting nearby. Its roots spread beneath the ground, while birds rest among the branches. The space under it stays cool on warm days.',
    ]],
    ['food', 'Food and cooking', [
      'The ingredients are ready before we light the stove. One person washes the vegetables while another measures the rice. Working together makes the meal easier to prepare.',
      'At the market, we choose fruit that is firm and fragrant. The seller helps us find a ripe pineapple for the weekend. We carry it home carefully in a reusable bag.',
      'The soup needs a little more time over low heat. I taste a spoonful, add a pinch of salt, and stir gently. When the vegetables are tender, it is ready to serve.',
      'A recipe calls for two cups of flour and one cup of water. We measure each ingredient before mixing them in a bowl. Clean hands and a clear workspace make the task simpler.',
      'There is enough food for everyone at the table. We pass the bowls around and give each person time to serve themselves. Conversation continues long after the plates are empty.',
      'For a quick snack, I wash an orange and slice it on a clean board. I put the pieces in a small container and close the lid. It is ready to take along later.',
      'After cooking, we let the pot cool before washing it. The spoons go into a drawer, and the cups return to their shelf. A tidy kitchen is ready for the next meal.',
    ]],
    ['travel', 'Journeys and directions', [
      'We leave early to avoid the busiest part of the morning. At the first junction, we check the sign and turn toward the bridge. The rest of the route is easy to follow.',
      'A visitor asks how to reach the reading room. I explain that it is past the courtyard, beside the blue door. If the door is closed, the front desk can help.',
      'The bus arrives while we are checking the schedule. We confirm the destination with the driver before boarding. A few stops later, we recognize the building we need.',
      'Our route takes us past a row of small shops. At the corner, we pause to let a bicycle pass and then cross the road. The community hall is just ahead on the left.',
      'We keep a note of the return time as well as the departure time. That makes it easier to plan the rest of the day. A small delay will not upset the whole schedule.',
      'The path to the garden has two entrances. One is near the water station, and the other is beside the painted wall. Both lead to the same shaded meeting area.',
      'I pack the map and a bottle of water before setting out. Along the way, I look for the landmarks mentioned in the directions. Each one confirms that I am on the right route.',
    ]],
    ['community', 'Community life', [
      'Neighbors bring brooms and bags to the cleanup day. Some collect litter while others clear leaves from the walkway. By noon, the entrance looks brighter and safer.',
      'A reading circle meets once a week in the shared room. Each visitor can bring a book or listen to someone else read. The group welcomes beginners as well as experienced readers.',
      'The garden team asks which plants would be most useful. People suggest herbs, vegetables, and flowers for the front path. Together they choose a few seeds to start with.',
      'Several volunteers arrive early to prepare the hall. They place signs at the entrance and leave space between the rows of chairs. Visitors can find their way without crowding.',
      'A notice board keeps the neighborhood informed about upcoming events. One person checks it each week and removes old notices. New messages include a date and a contact name.',
      'The music stops for a moment so the host can thank everyone. People who prepared food, arranged seats, and welcomed guests are recognized. Their shared effort made the gathering possible.',
      'A small library receives a box of donated books. The volunteers sort them by topic and check that the pages are intact. Soon the books are ready for readers to borrow.',
    ]],
  ];

  const practicalSets = [
    ['messages', 'Friendly messages', [
      'Hello! I hope your morning is going well. Let me know when you arrive.',
      'Thank you for the helpful notes. I will read them this evening.',
      'Could we talk after lunch? I have a question about the plan.',
      'I enjoyed our conversation today. Thank you for making time to meet.',
      'Please send a short update when the materials are ready.',
      'I may be a few minutes late, but I will be there as soon as I can.',
      'The books are ready to collect. Would tomorrow be convenient?',
      'Thanks for checking on the project. Everything is moving along well.',
      'I have a new idea to share. We can discuss it at our next meeting.',
      'Have a restful evening, and thank you again for your help.',
    ]],
    ['schedule', 'Dates and times', [
      'The meeting begins at 9:30 a.m. on Monday in the main room.',
      'Please arrive by 2:15 p.m. so we can begin on time.',
      'The library visit is planned for Friday at 10:00 a.m.',
      'Our practice session runs from 3:00 to 3:45 in the afternoon.',
      'The report is due on April 12. A draft will be ready on April 8.',
      'We will take a short break at 11:20 and return at 11:35.',
      'The workshop starts on June 4 and continues for three days.',
      'Set aside 15 minutes on Tuesday to review the notes.',
      'The next group call is at 4:30 p.m. on Thursday.',
      'Please check the calendar for the updated date and time.',
    ]],
    ['shopping', 'Shopping lists', [
      'Shopping list: 3 oranges, 2 loaves of bread, and 1 bag of rice.',
      'The sample budget includes $4.50 for fruit and $2.25 for bread.',
      'Please compare the prices before choosing the larger package.',
      'We need 6 cups, 4 plates, and 2 clean serving bowls.',
      'The total is $12.75. Count the change before leaving the table.',
      'Add soap, cooking oil, and a small packet of tea to the list.',
      'Choose fresh tomatoes, onions, and greens for the evening meal.',
      'There are 5 notebooks in each pack and 3 packs on the shelf.',
      'The first item costs $1.20, and the second item costs $3.40.',
      'Bring a reusable bag and keep the receipt with the shopping notes.',
    ]],
    ['directions', 'Directions', [
      'Walk past the market, then turn left at the tall green gate.',
      'The reading room is on the right, just beyond the main entrance.',
      'Follow the road to the bridge and turn toward the small park.',
      'Go straight for two blocks, then look for the sign by the library.',
      'The meeting hall is behind the school, near the open courtyard.',
      'Take the first path on the left and continue to the water station.',
      'When you reach the corner, cross carefully and turn right.',
      'The office is upstairs, at the end of the hallway.',
      'Pass the large tree and follow the path to the community garden.',
      'Ask at the front desk if you need help finding the room.',
    ]],
    ['projects', 'Project notes', [
      'Project goal: prepare a clear plan and share it with the team.',
      'First, list the materials. Next, decide who will bring each item.',
      'The first draft is ready for review. Please add comments by Friday.',
      'We have finished the outline; the next step is to check the details.',
      'Action item: confirm the location and update the shared schedule.',
      'The team agreed to meet weekly until the work is complete.',
      'Keep one copy of the notes and share another with the group.',
      'The project has three stages: planning, preparation, and review.',
      'Please check the figures before adding them to the final report.',
      'We will review the results together and decide what to improve.',
    ]],
    ['study-notes', 'Study notes', [
      'Main idea: practice a little each day and review what you learn.',
      'Question: What is the first step? Answer: Read the instructions.',
      'Write three key points, then explain each one in your own words.',
      'Remember to check the date, title, and page number on your notes.',
      'A useful summary includes the topic, the main idea, and an example.',
      'Study plan: read for 20 minutes, rest, and review for 10 minutes.',
      'Compare the two examples and note one way they are different.',
      'Circle the words you do not know and ask about them later.',
      'Use a short heading to separate each section of your notes.',
      'Before finishing, check your answer against the original question.',
    ]],
    ['files', 'Files and folders', [
      'Save the first draft as notes.txt in the project folder.',
      'Create a folder called practice and keep related files inside it.',
      'Rename the document from draft-one.txt to final-notes.txt.',
      'Open the file, make the requested change, and save a new copy.',
      'Use a clear title so the document is easy to recognize later.',
      'The folder contains a schedule, a list, and a short report.',
      'Check the file name before moving the document to a new folder.',
      'Keep older drafts separate from the version ready to share.',
      'A backup copy can help if a file is changed by mistake.',
      'Close the document when the work is complete and saved.',
    ]],
    ['events', 'Events and gatherings', [
      'The community event begins at 10:00. Guests are welcome from 9:45.',
      'Please place the chairs in rows and leave a clear path to the door.',
      'The music will begin after the opening welcome and announcements.',
      'We need two tables for registration and one table for supplies.',
      'A volunteer will show visitors where to find the meeting room.',
      'The program includes a short talk, a break, and a group discussion.',
      'Please return the markers and paper to the supply table afterward.',
      'The event ends at 4:00 p.m.; cleanup starts immediately afterward.',
      'A printed schedule will be available near the main entrance.',
      'Thank you to everyone who helped prepare a welcoming space.',
    ]],
    ['requests', 'Helpful requests', [
      'Could you please show me where to find the registration form?',
      'May I borrow a pen while I complete this short application?',
      'Please let me know if the meeting time has changed.',
      'Would you explain the next step once more? I want to understand it.',
      'Could we arrange a time to review the notes together?',
      'Please keep a copy of the receipt with the other documents.',
      'Would it be possible to move the chair closer to the table?',
      'Could you check the list and tell me if anything is missing?',
      'Please share the final schedule when it is ready.',
      'Thank you for your time. Your explanation was very helpful.',
    ]],
    ['checklists', 'Lists and reminders', [
      'Before leaving: check the door, collect the keys, and take the list.',
      'Reminder: send the update, file the notes, and prepare for tomorrow.',
      'For the class, bring a notebook, two pencils, and a bottle of water.',
      'Review the title, spelling, numbers, and punctuation before sharing.',
      'The morning checklist has four items: read, plan, practice, and rest.',
      'Put the completed forms in the folder marked ready for review.',
      'Remember to leave space in the schedule for a short break.',
      'Check that the tables are clean and the chairs are in place.',
      'After the meeting, write down the decisions and the next steps.',
      'A simple reminder can make a busy day easier to manage.',
    ]],
  ];

  const capitalWords = {
    a: 'Apple Anchor Apron Awake After', b: 'Bloom Bright Bubble Basket Bridge',
    c: 'Calm Cloud Circle Candle Careful', d: 'Day Dream Dance Dinner Decide',
    e: 'Early Eager Even Enjoy Explore', f: 'Focus Flower Fresh Friend Forest',
    g: 'Garden Gentle Grow Green Gather', h: 'Happy Home Hello Heart Helpful',
    i: 'Idea Island Invite Imagine Inside', j: 'Jazz Journey Joy Jump Jewel',
    k: 'Kind Keyboard Keep Kitchen Kindness', l: 'Light Learn Little Listen Lovely',
    m: 'Morning Music Make Moment Memory', n: 'Nature New Number Notes Neighbor',
    o: 'Open Orange Ocean Outside Ordinary', p: 'Paper Practice Path People Project',
    q: 'Quiet Quick Queen Question Quote', r: 'River Ready Rhythm Rest Remember',
    s: 'Smile Steady Simple Story Sunshine', t: 'Time Together Today Task Tomorrow',
    u: 'Useful Under Up Umbrella United', v: 'Value Vivid Visit View Village',
    w: 'Window Warm Welcome Water Wonderful', x: 'X-ray Xylophone Xenon X-axis Xylem',
    y: 'Yellow Young Your Yesterday Year', z: 'Zebra Zero Zone Zoom Zipper',
  };
  function numbered(n) { return String(n + 1).padStart(2, '0'); }
  function duration(text) { return Math.max(1, Math.min(4, Math.ceil(text.length / 100))); }
  function make(id, group, title, keys, tip, text) {
    return { id, group, title, keys, tip, text, minutes: duration(text) };
  }

  function createExtendedLessons() {
    const entries = [];
    const letterStyles = [
      ['warm', 'Letter warm-ups', 'Warm-up', 'Take a relaxed approach to the highlighted letter.',
        (key, words) => `${key} ${key} ${key}${key} ${key} ${words[0]} ${words[1]} ${words[2]} ${key}${key} ${words[3]}`],
      ['flow', 'Letter combinations', 'Word flow', 'Move smoothly between the highlighted letter and its neighbors.',
        (_key, words) => [words[3], words[4], words[0], words[5], words[1], words[6], words[2], words[7]].join(' ')],
      ['words', 'Letter word practice', 'Word collection', 'Try the highlighted letter in a variety of everyday words.',
        (_key, words) => [words[6], words[7], words[8], words[9], words[0], words[2], words[4], words[6]].join(' ')],
      ['capital', 'Capital letter practice', 'Capital letters', 'Practice Shift gently, then return your fingers to the home row.',
        (key) => `${key.toUpperCase()} ${key} ${key.toUpperCase()} ${capitalWords[key]}.`],
      ['sentence', 'Letter sentences', 'A sentence', 'Look for the highlighted letter as you type this complete sentence.',
        (_key, _words, sentence) => sentence],
    ];
    for (const [style, group, name, tip, build] of letterStyles) {
      for (const [key, list, sentence] of letterSets) {
        const words = list.split(' ');
        const text = build(key, words, sentence);
        entries.push(make(`letter-${style}-${key}`, group, `${key.toUpperCase()} · ${name}`, key.toUpperCase(), tip, text));
      }
    }

    const numberStyles = [
      ['warm', 'Digit warm-up', 'Find the number key gently and return to the home row.',
        (d) => `${d} ${d} ${d}${d} ${d}${d} ${d}${d}${d} ${d} ${d}${d} ${d} ${d}${d}`],
      ['pairs', 'Number pairs', 'Aim for accuracy as you move along the number row.',
        (d) => Array.from({ length: 10 }, (_, i) => `${d}${i}`).join(' ')],
      ['count', 'Counting', 'Read each number before moving to the next one.',
        (d) => `Count: ${Array.from({ length: 8 }, (_, i) => Number(d) + i).join(' ')}.`],
      ['time', 'Clock times', 'Use Shift for the colon and take your time with each number.',
        (d) => { const hour = d === '0' ? 10 : Number(d); return `Times: ${hour}:00, ${hour}:15, ${hour}:30, ${hour}:45.`; }],
      ['prices', 'Prices', 'Practice the dollar sign, the decimal point, and the number row.',
        (d) => `Prices: $${d}.10, $${d}.25, $${d}.50, $${d}.75, $${d}.99.`],
    ];
    for (const [style, title, tip, build] of numberStyles) {
      for (const digit of '0123456789') {
        const text = build(digit);
        entries.push(make(`number-${style}-${digit}`, 'Numbers and counting', `${title} · ${digit}`, `${digit} · 0–9`, tip, text));
      }
    }

    for (const [id, label, key, texts] of punctuationSets) {
      texts.forEach((text, index) => entries.push(make(`punct-${id}-${numbered(index)}`, 'Punctuation practice',
        `${label} · ${numbered(index)}`, key, 'Watch the punctuation and aim for a steady rhythm.', text)));
    }
    for (const [id, label, texts] of everydaySets) {
      texts.forEach((text, index) => entries.push(make(`everyday-${id}-${numbered(index)}`, 'Everyday sentences',
        `${label} · ${numbered(index)}`, 'SENTENCES', 'Read the whole sentence, then type at a comfortable pace.', text)));
    }
    for (const [id, label, texts] of practicalSets) {
      texts.forEach((text, index) => entries.push(make(`real-${id}-${numbered(index)}`, 'Real-world practice',
        `${label} · ${numbered(index)}`, 'EVERYDAY', 'Practice the words, numbers, and punctuation as they appear.', text)));
    }
    for (const [id, label, texts] of paragraphSets) {
      texts.forEach((text, index) => entries.push(make(`paragraph-${id}-${numbered(index)}`, 'Paragraph practice',
        `${label} · paragraph ${numbered(index)}`, 'PARAGRAPH', 'Keep a comfortable rhythm through the longer passage.', text)));
    }
    return entries;
  }

  return { createExtendedLessons, letterSets, capitalWords, punctuationSets, everydaySets, practicalSets, paragraphSets };
});
