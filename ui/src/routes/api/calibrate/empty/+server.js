/** @type {import('./$types').RequestHandler} */
export async function POST() {
	let responseBody = { message: 'Begin calibration success' };
	return new Response(JSON.stringify(responseBody), { status: 200 });
}
